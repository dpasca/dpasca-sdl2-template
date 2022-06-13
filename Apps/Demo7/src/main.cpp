//==================================================================
/// main.cpp
///
/// Created by Davide Pasca - 2022/06/12
/// See the file "license.txt" that comes with this project for
/// copyright info.
//==================================================================

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <array>
#include <vector>
#include <algorithm> // for std::sort
#include "DBase.h"
#include "MathBase.h"
#include "Plasma2.h"
#include "MU_WrapMap.h"
#include "MU_ParallelOcclChecker.h"
#include "MinimalSDLApp.h"

//==================================================================
static constexpr float TERR_DISPW       = 10.f;

static constexpr float DISP_CAM_NEAR    = 0.01f;    // near plane (1 cm)
static constexpr float DISP_CAM_FAR     = 1000.f;   // far plane (1000 m)

static constexpr Float3 CHROM_LAND      { 0.8f , 0.7f , 0.0f }; // chrominance for land
static constexpr Float3 CHROM_SEA       { 0.0f , 0.6f , 0.9f }; // chrominance for sea

static const     Float3 LIGHT_DIR       { glm::normalize( Float3( 0.2f, 0.06f, 0.2f ) ) };

struct DemoParams
{
    float       DISP_CAM_FOV_DEG    = 65.f;       // field of view
    float       DISP_CAM_DIST       = TERR_DISPW; // distance from center
    Float2      DISP_CAM_PY_ANGS    = {20.f, 0.f};
    bool        DISP_ANIM_YAW       = true;
    uint32_t    DISP_CROP_WH[2]     = {0,0};

    float       GEN_MIN_H           = -TERR_DISPW / 15.f;
    float       GEN_MAX_H           =  TERR_DISPW / 10.f;
    uint32_t    GEN_SIZL2           = 7;       // 128 x 128 map
    uint32_t    GEN_STASIZL2        = 2;       // 4 x 4 initial random samples
    uint32_t    GEN_SEED            = 100;     // random seed
    float       GEN_ROUGH           = 0.5f;
    bool        GEN_APPLY_SHADOWS   = true;
    bool        GEN_WRAP_EDGES      = false;
};

static DemoParams   _sPar;

//
using ColType = glm::vec<4, uint8_t, glm::defaultp>;

//==================================================================
inline float DEG2RAD( float deg )
{
    return glm::pi<float>() / 180.f * deg;
}

//==================================================================
//=== Rendering
//==================================================================
// vertex coming from the object
struct VertObj
{
    Float3      pos;
    float       siz;
    ColType     col;
};
// vertex output in screen space
struct VertDev
{
    Float3      pos {};
    Float2      siz {};
    ColType     col {};
};

//==================================================================
inline VertDev makeDeviceVert(
                    const Matrix44 &xform,
                    const VertObj &vobj,
                    float deviceW,
                    float deviceH )
{
    VertDev vdev;

    // homogeneus coordinates (-w..w)
    c_auto posH = xform * glm::vec4( vobj.pos, 1.f );

    if ( posH[2] <= 0 ) // skip if it's behind the camera
        return vdev;

    // convert to screen-space, meaning that anything visible is
    // in the range -1..1 for x,y and 0..1 for z
    c_auto oow = 1.f / posH.w;

    vdev.pos[0] = deviceW * (posH[0] * oow + 1) * 0.5f;
    vdev.pos[1] = deviceH * (1 - posH[1] * oow) * 0.5f;
    vdev.pos[2] = posH[2] * oow;

    vdev.siz[0] = deviceW * vobj.siz * oow;
    vdev.siz[1] = deviceH * vobj.siz * oow;

    vdev.col = vobj.col;

    return vdev;
}

//
inline bool isValidDeviceVert( const Float3 &vert )
{
    return vert[2] != 0.f;
}

//==================================================================
inline void drawAtom( auto *pRend, const VertDev &vdev )
{
    c_auto c = vdev.col;
    SDL_SetRenderDrawColor( pRend, c[0], c[1], c[2], c[3] );
    SDL_FRect rc;
    c_auto w = vdev.siz[0];
    c_auto h = vdev.siz[1];
    rc.x = (float)(vdev.pos[0] - w*0.5f);
    rc.y = (float)(vdev.pos[1] - h*0.5f);
    rc.w = w;
    rc.h = h;
    SDL_RenderFillRectF( pRend, &rc );
}

//==================================================================
//=== Generation
//==================================================================

//==================================================================
class Terrain
{
public:
    size_t                  mSizeL2 {};
    std::vector<float>      mHeights;
    std::vector<ColType>    mCols;
    float                   mMinH   {0};
    float                   mMaxH   {1.5f};

    Terrain() {}

    Terrain( size_t sizeL2 )
        : mSizeL2(sizeL2)
        , mHeights( (size_t)1 << ((int)sizeL2 * 2) )
    {
    }

    size_t GetDim() const { return (size_t)1 << mSizeL2; }

    size_t MakeIndexXY( size_t x, size_t y ) const { return x + (y << mSizeL2); }

    void DrawTerrain(
            float mapDispW,
            const uint32_t cropWH[2],
            auto *pRend,
            float deviceW,
            float deviceH,
            const Matrix44 &proj_obj ) const
    {
        c_auto dim = (size_t)1 << mSizeL2;

        c_auto dxdt = mapDispW / (float)dim;
        c_auto xoff = -mapDispW / 2;

        // define the usable crop area (all if 0, otherwise no larger than the map's dim)
        c_auto useCropW = cropWH[0] ? std::min( (size_t)cropWH[0], dim ) : dim;
        c_auto useCropH = cropWH[1] ? std::min( (size_t)cropWH[1], dim ) : dim;

        c_auto xi1 = (dim - useCropW) / 2;
        c_auto xi2 = (dim + useCropW) / 2;
        c_auto yi1 = (dim - useCropH) / 2;
        c_auto yi2 = (dim + useCropH) / 2;

        std::vector<VertDev> vertsDev;
        vertsDev.reserve( (yi2 - yi1) * (xi2 - xi1) );

        size_t cellIdx = 0;
        for (size_t yi=yi1; yi < yi2; ++yi)
        {
            c_auto y = xoff + dxdt * (yi + 1);

            c_auto rowCellIdx = yi << mSizeL2;
            for (size_t xi=xi1; xi < xi2; ++xi)
            {
                c_auto x = xoff + dxdt * (xi + 1);

                VertObj vobj;
                vobj.pos = {x, mHeights[ xi + rowCellIdx ], y};
                vobj.siz = dxdt;
                vobj.col = mCols[ xi + rowCellIdx ];

                // convert from object-space to device-space (2D display dimensions)
                c_auto vout = makeDeviceVert( proj_obj, vobj, deviceW, deviceH );

                // store the vertex
                if ( vout.pos[2] > 0 )
                    vertsDev.push_back( vout );
            }
        }

        // sort with bigger Z first
        std::sort( vertsDev.begin(), vertsDev.end(), []( c_auto &l, c_auto &r )
        {
            return l.pos[2] > r.pos[2];
        });

        // finally render the verts
        for (c_auto &v : vertsDev)
            drawAtom( pRend, v );
    }
};

//==================================================================
static void TERR_ScaleHeights( auto &terr, float newMin, float newMax )
{
    auto *p = terr.mHeights.data();
    c_auto n = (size_t)1 << ((int)terr.mSizeL2 * 2);

    float mi =  FLT_MAX;
    float ma = -FLT_MAX;
    for (size_t i=0; i < n; ++i)
    {
        mi = std::min( mi, p[i] );
        ma = std::max( ma, p[i] );
    }

    // rescale and offset
    c_auto scaToNew = (ma != mi) ? ((newMax - newMin) / (ma - mi)) : 0.f;
    for (size_t i=0; i < n; ++i)
        p[i] = newMin + (p[i] - mi) * scaToNew;

    // update the terrain values
    terr.mMinH = newMin;
    terr.mMaxH = newMax;
}

//==================================================================
inline auto remapRange( c_auto &v, c_auto &srcL, c_auto &srcR, c_auto &desL, c_auto &desR )
{
    c_auto t = (v - srcL) / (srcR - srcL);
    return glm::mix( desL, desR, t );
}

//==================================================================
// geenrate colors and flatten the heights below sea level
static void TERR_Colorize( auto &terr )
{
    terr.mCols.resize( terr.mHeights.size() );
    for (size_t i=0; i < terr.mHeights.size(); ++i)
    {
        auto &h = terr.mHeights[i];

        // chrominance
        c_auto chrom = h >= 0 ? CHROM_LAND : CHROM_SEA;
        // luminance from min-max range to a suitable range for coloring
        c_auto lum = remapRange( h, terr.mMinH, terr.mMaxH, 40.f, 255.f );
        // complete color is luminance by chrominance
        c_auto col = lum * chrom;

        // assign the color to the map
        terr.mCols[i] = { (uint8_t)col[0], (uint8_t)col[1], (uint8_t)col[2], 255 };

        // flatted to the height map at the sea level !
        h = std::max( h, 0.f );
    }
}

//==================================================================
// geenrate colors and flatten the heights below sea level
static void TERR_CalcShadows( auto &terr, Float3 lightDirWS )
{
    auto checker = MU_ParallelOcclChecker(
                        terr.mHeights.data(),
                        lightDirWS,
                        terr.mMinH,
                        terr.mMaxH,
                        terr.mSizeL2 );

    c_auto dim = terr.GetDim();
	for (size_t yi=0; yi < dim; ++yi)
	{
        for (size_t xi=0; xi < dim; ++xi)
		{
			if ( checker.IsOccludedAtPoint( (int)xi, (int)yi ) )
            {
                auto &c = terr.mCols[ terr.MakeIndexXY( xi, yi ) ];
                c[0] /= 2; // make half bright
                c[1] /= 2;
                c[2] /= 2;
            }
		}
	}
}

//==================================================================
static void TERR_MakeFromParams( auto &terr )
{
    // allocate a new map
    terr = Terrain( _sPar.GEN_SIZL2 );

    // fill it with "plasma"
    Plasma2::Params par;
    par.pDest       = terr.mHeights.data(); // destination values
    par.sizL2       = terr.mSizeL2;         // log2 of size (i.e. 7 = 128 pixels width/height)
    par.baseSizL2   = _sPar.GEN_STASIZL2;// log2 of size of initial low res map
    par.seed        = _sPar.GEN_SEED;
    par.rough       = _sPar.GEN_ROUGH;

    // generate the map
    Plasma2 plasma( par );
    while ( plasma.IterateRow() )
    {
    }

    // transform heights to the required range
    TERR_ScaleHeights( terr, _sPar.GEN_MIN_H, _sPar.GEN_MAX_H );

    // blend edges to make a continuous map
    if ( _sPar.GEN_WRAP_EDGES )
    {
        // we wrap by cross-blending the extreme 1/3 of the samples at the edges
        c_auto wrapSiz = ((size_t)1 << terr.mSizeL2) / 3;
        MU_WrapMap<float,1>( terr.mHeights.data(), terr.mSizeL2, wrapSiz );
    }

    // apply colors
    TERR_Colorize( terr );

    // apply shadow dimming
    if ( _sPar.GEN_APPLY_SHADOWS )
        TERR_CalcShadows( terr, LIGHT_DIR );
}

#ifdef ENABLE_IMGUI
//==================================================================
static void handleUI( size_t frameCnt, Terrain &terr )
{
    //ImGui::Text( "Frame: %zu", frameCnt );

    auto header = []( c_auto *pName )
    {
        return ImGui::CollapsingHeader( pName, ImGuiTreeNodeFlags_DefaultOpen );
    };

    if ( header( "Display" ) )
    {
        ImGui::SliderFloat( "Camera FOV", &_sPar.DISP_CAM_FOV_DEG, 10.f, 120.f );
        ImGui::SliderFloat( "Camera Dist", &_sPar.DISP_CAM_DIST, 0.f, DISP_CAM_FAR/10 );
        ImGui::SliderFloat2( "Camera Pitch/Yaw", &_sPar.DISP_CAM_PY_ANGS[0], -180, 180 );

        ImGui::Checkbox( "Anim Yaw", &_sPar.DISP_ANIM_YAW );

        ImGui::InputScalarN( "Crop", ImGuiDataType_U32, _sPar.DISP_CROP_WH, 2 );
    }

    if ( header( "Generation" ) )
    {
        auto inputU32 = []( c_auto *pName, uint32_t *pVal, uint32_t step )
        {
            return ImGui::InputScalar( pName, ImGuiDataType_U32, pVal, &step, nullptr, "%d" );
        };
        auto slideU32 = []( c_auto *pName, uint32_t *pVal, uint32_t mi, uint32_t ma )
        {
            return ImGui::SliderScalar( pName, ImGuiDataType_U32, pVal, &mi, &ma, nullptr, 0 );
        };

        bool rebuild = false;
        rebuild |= ImGui::InputFloat( "Max Height", &_sPar.GEN_MAX_H, 0.01f, 0.1f );
        rebuild |= ImGui::InputFloat( "Min Height", &_sPar.GEN_MIN_H, 0.01f, 0.1f );
        rebuild |= slideU32( "Size Log2", &_sPar.GEN_SIZL2, 0, 9 );
        rebuild |= slideU32( "Init Size Log2", &_sPar.GEN_STASIZL2, 0, _sPar.GEN_SIZL2 );
        rebuild |= ImGui::InputFloat( "Roughness", &_sPar.GEN_ROUGH, 0.01f, 0.1f );
        rebuild |= inputU32( "Seed", &_sPar.GEN_SEED, 1 );
        rebuild |= ImGui::Checkbox( "Apply Shadows", &_sPar.GEN_APPLY_SHADOWS );
        rebuild |= ImGui::Checkbox( "Wrap Edges", &_sPar.GEN_WRAP_EDGES );

        if ( rebuild )
        {
            _sPar.GEN_STASIZL2 = std::min( _sPar.GEN_STASIZL2, _sPar.GEN_SIZL2 );
            _sPar.GEN_ROUGH    = std::clamp( _sPar.GEN_ROUGH, 0.f, 1.f );

            TERR_MakeFromParams( terr );
        }
    }
}
#endif

//==================================================================
inline void debugDraw(
                auto *pRend,
                float deviceW,
                float deviceH,
                const Matrix44 &proj_obj )
{
    auto draw3DLine = [&]( const Float3 &pos1, const Float3 &pos2 )
    {
        // convert from object-space to device-space (2D display dimensions)
        c_auto v1 = makeDeviceVert( proj_obj, VertObj{pos1}, deviceW, deviceH );
        c_auto v2 = makeDeviceVert( proj_obj, VertObj{pos2}, deviceW, deviceH );

        c_auto v1p = v1.pos;
        c_auto v2p = v2.pos;

        if ( v1p[2] > 0 && v2p[2] > 0 )
            SDL_RenderDrawLineF( pRend, v1p[0], v1p[1], v2p[0], v2p[1] );
    };

    SDL_SetRenderDrawColor( pRend, 0, 255, 0, 90 );

    draw3DLine( Float3(0,0,0), LIGHT_DIR * TERR_DISPW * 0.5f );
}

//==================================================================
int main( int argc, char *argv[] )
{
    constexpr int  W = 1024;
    constexpr int  H = 768;

    MinimalSDLApp app( argc, argv, W, H );

    Terrain terr;
    TERR_MakeFromParams( terr );

    // begin the main/rendering loop
    for (size_t frameCnt=0; ; ++frameCnt)
    {
        // begin the frame (or get out)
        if ( !app.BeginFrame() )
            break;

#ifdef ENABLE_IMGUI
        app.DrawMainUIWin( [&]() { handleUI( frameCnt, terr ); } );
#endif
        // get the renderer
        auto *pRend = app.GetRenderer();

        // clear the device
        SDL_SetRenderDrawColor( pRend, 0, 0, 0, 0 );
        SDL_RenderClear( pRend );

        // animate
        if ( _sPar.DISP_ANIM_YAW )
        {
            // increase and wrap
            _sPar.DISP_CAM_PY_ANGS[1] += 0.5f;
            _sPar.DISP_CAM_PY_ANGS[1] = fmodf( _sPar.DISP_CAM_PY_ANGS[1]+180, 360.f ) - 180;
        }

        // obj -> world matrix
        auto world_obj = Matrix44( 1.f );

        // camera -> world matrix
        auto cam_world = [&]()
        {
            auto m = Matrix44( 1.f );
            m = glm::translate( m, Float3(0.0f, 0.0f, -_sPar.DISP_CAM_DIST) );
            m = glm::rotate( m, DEG2RAD(_sPar.DISP_CAM_PY_ANGS[0]), Float3(1, 0, 0) );
            m = glm::rotate( m, DEG2RAD(_sPar.DISP_CAM_PY_ANGS[1]), Float3(0, 1, 0) );
            return m;
        }();

        // camera -> projection matrix
        c_auto proj_camera = glm::perspective(
                                DEG2RAD( _sPar.DISP_CAM_FOV_DEG ),  // FOV
                                (float)W / H,                       // aspect ratio
                                DISP_CAM_NEAR,
                                DISP_CAM_FAR );

        // obj -> proj matrix
        c_auto proj_obj = proj_camera * cam_world * world_obj;

        // draw the terrain
        terr.DrawTerrain(
                TERR_DISPW,
                _sPar.DISP_CROP_WH,
                pRend,
                W,
                H,
                proj_obj );

        //
        //debugDraw( pRend, W, H, proj_obj );

        // end of the frame (will present)
        app.EndFrame();
    }

    return 0;
}

