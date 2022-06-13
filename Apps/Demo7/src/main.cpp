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
#include "WrapMap.h"
#include "MinimalSDLApp.h"

//==================================================================
static constexpr float HMAP_DISPW       = 10.f;
static constexpr float HMAP_MIN_H       = -HMAP_DISPW / 15.f;
static constexpr float HMAP_MAX_H       =  HMAP_DISPW / 10.f;

static constexpr float CAMERA_DIST      = HMAP_DISPW; // distance from center
static constexpr float CAMERA_NEAR      = 0.01f;    // near plane (1 cm)
static constexpr float CAMERA_FAR       = 1000.f;   // far plane (1000 m)

static constexpr Float3 CHROM_LAND      { 0.8f , 0.7f , 0.0f }; // chrominance for land
static constexpr Float3 CHROM_SEA       { 0.0f , 0.6f , 0.9f }; // chrominance for sea

struct DemoParams
{
    bool     ANIM_OBJ_POS    = false;
    float    CAMERA_FOV_DEG  = 65.f;    // field of view

    uint32_t PLASMA_SIZL2    = 7;       // 128 x 128 map
    uint32_t PLASMA_STASIZL2 = 2;       // 4 x 4 initial random samples
    uint32_t PLASMA_SEED     = 100;     // random seed
    float    PLASMA_ROUGH    = 0.5f;

    bool     WRAPPED_EDGES   = false;
};

static DemoParams   _sPar;

//==================================================================
using ColType = std::array<uint8_t,4>;

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
    const auto posH = xform * glm::vec4( vobj.pos, 1.f );

    if ( posH[2] <= 0 ) // skip if it's behind the camera
        return vdev;

    // convert to screen-space, meaning that anything visible is
    // in the range -1..1 for x,y and 0..1 for z
    const auto oow = 1.f / posH.w;

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
static ColType makeCol( float h, float dispH )
{
    c_auto lum = glm::mix( 40.f, 255.f, h );

    // chrominance
    c_auto chrom = dispH >= 0 ? CHROM_LAND : CHROM_SEA;

    c_auto col = lum * chrom;

    return { (uint8_t)col[0], (uint8_t)col[1], (uint8_t)col[2], 255 };
}

//==================================================================
class HMap
{
public:
    size_t              mSizeL2 {};
    std::vector<float>  mHeights;

    HMap() {}

    HMap( size_t sizeL2 )
        : mSizeL2(sizeL2)
        , mHeights( (size_t)1 << ((int)sizeL2 * 2) )
    {
    }

    void DrawHMap(
            float mapDispW,
            float mapDispMinH,
            float mapDispMaxH,
            auto *pRend,
            float deviceW,
            float deviceH,
            const Matrix44 &proj_obj ) const
    {
        std::vector<VertDev> vertsDev;

        c_auto dim = 1 << mSizeL2;

        c_auto dxdt = mapDispW / (float)dim;
        c_auto xoff = -mapDispW / 2;

        size_t cellIdx = 0;
        for (size_t yi=0; yi < dim; ++yi)
        {
            c_auto y = xoff + dxdt * (yi + 1);
            for (size_t xi=0; xi < dim; ++xi, ++cellIdx)
            {
                c_auto x = xoff + dxdt * (xi + 1);

                c_auto h = mHeights[ cellIdx ];
                c_auto dispH = glm::mix( mapDispMinH, mapDispMaxH, h );

                VertObj vobj;
                // height goes no lower than 0 (simulate water)
                vobj.pos = {x, std::max( dispH, 0.f ), y};
                vobj.siz = dxdt;
                vobj.col = makeCol( h, dispH );

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
        for (const auto &v : vertsDev)
            drawAtom( pRend, v );
    }
};

//==================================================================
inline float DEG2RAD( float deg )
{
    return glm::pi<float>() / 180.f * deg;
}

//==================================================================
static void hmap_MakeFromParams( auto &hmap )
{
    // allocate a new map
    hmap = HMap( _sPar.PLASMA_SIZL2 );

    // fill it with "plasma"
    Plasma2::Params par;
    par.pDest       = hmap.mHeights.data(); // destination values
    par.sizL2       = hmap.mSizeL2;         // log2 of size (i.e. 7 = 128 pixels width/height)
    par.baseSizL2   = _sPar.PLASMA_STASIZL2;// log2 of size of initial low res map
    par.seed        = _sPar.PLASMA_SEED;
    par.rough       = _sPar.PLASMA_ROUGH;

    // generate the map
    Plasma2 plasma( par );
    while ( plasma.IterateRow() )
    {
    }

    // normalize the height values from 0.0 to 1.0
    plasma.ScaleResults( 0.f, 1.f );

    // blend edges to make a continuous map
    if ( _sPar.WRAPPED_EDGES )
        WrapMap<float,1>( hmap.mHeights.data(),              // data
                          hmap.mSizeL2,                      // log2 size
                          ((size_t)1 << hmap.mSizeL2) / 3 ); // length to wrap
}

#ifdef ENABLE_IMGUI
//==================================================================
static void handleUI( size_t frameCnt, HMap &hmap )
{
    ImGui::Text( "Frame: %zu", frameCnt );
    ImGui::Checkbox( "Animate obj position", &_sPar.ANIM_OBJ_POS );

    ImGui::InputFloat( "Camera FOV", &_sPar.CAMERA_FOV_DEG, 0.5f, 0.10f );
    _sPar.CAMERA_FOV_DEG = std::clamp( _sPar.CAMERA_FOV_DEG, 10.f, 120.f );

    ImGui::NewLine();

    auto inputU32 = []( c_auto *pName, uint32_t *pVal, uint32_t step )
    {
        return ImGui::InputScalar( pName, ImGuiDataType_S32, pVal, &step, nullptr, "%d" );
    };

    bool rebuild = false;
    rebuild |= inputU32( "Size Log2", &_sPar.PLASMA_SIZL2, 1 );
    rebuild |= inputU32( "Init Size Log2", &_sPar.PLASMA_STASIZL2, 1 );
    rebuild |= ImGui::InputFloat( "Roughness", &_sPar.PLASMA_ROUGH, 0.01f, 0.1f );
    rebuild |= inputU32( "Seed", &_sPar.PLASMA_SEED, 1 );
    rebuild |= ImGui::Checkbox( "Wrap Edges", &_sPar.WRAPPED_EDGES );

    if ( rebuild )
    {
        _sPar.PLASMA_SIZL2    = std::clamp( _sPar.PLASMA_SIZL2, (uint32_t)0, (uint32_t)9 );
        _sPar.PLASMA_STASIZL2 = std::clamp( _sPar.PLASMA_STASIZL2,
                                                        (uint32_t)0, _sPar.PLASMA_SIZL2 );

        _sPar.PLASMA_ROUGH    = std::clamp( _sPar.PLASMA_ROUGH, 0.f, 1.f );

        hmap_MakeFromParams( hmap );
    }
}
#endif

//==================================================================
int main( int argc, char *argv[] )
{
    constexpr int  W = 800;
    constexpr int  H = 600;

    MinimalSDLApp app( argc, argv, W, H );

    HMap hmap;
    hmap_MakeFromParams( hmap );

    // begin the main/rendering loop
    for (size_t frameCnt=0; ; ++frameCnt)
    {
        // begin the frame (or get out)
        if ( !app.BeginFrame() )
            break;

#ifdef ENABLE_IMGUI
        app.DrawMainUIWin( [&]() { handleUI( frameCnt, hmap ); } );
#endif
        // get the renderer
        auto *pRend = app.GetRenderer();

        // clear the device
        SDL_SetRenderDrawColor( pRend, 0, 0, 0, 0 );
        SDL_RenderClear( pRend );

        // --- OBJECT MATRIX ---
        const auto objAngY = (float)((double)frameCnt / 200.0); // in radiants
        // start with identity matrix
        auto world_obj = Matrix44( 1.f );

        if ( _sPar.ANIM_OBJ_POS )
        {
            // move the object on the Z
            c_auto objZ =
                CAMERA_DIST/2 * glm::mix( -0.3f, 0.5f, ((sin( frameCnt / 250.0 )+1)/2) );

            world_obj = glm::translate( world_obj, Float3(0.0f, 0.0f, objZ) );
        }

        // rotate the object
        world_obj = glm::rotate( world_obj, DEG2RAD( 15.f ), Float3( 1, 0, 0 ) );
        world_obj = glm::rotate( world_obj, objAngY, Float3( 0, 1, 0 ) );

        // --- CAMERA MATRIX ---
        // start with identity matrix
        auto camera_world = Matrix44( 1.f );
        // concatenate translation on the Z
        camera_world = glm::translate( camera_world, Float3(0.0f, 0.0f, -CAMERA_DIST) );

        // --- PROJECTION MATRIX ---
        // camera -> projection
        const auto proj_camera = glm::perspective(
                                    DEG2RAD( _sPar.CAMERA_FOV_DEG ),  // FOV
                                    (float)W / H,               // aspect ratio
                                    CAMERA_NEAR,
                                    CAMERA_FAR );

        // --- FINAL MATRIX ---
        // transforming obj -> projection
        const auto proj_obj = proj_camera * camera_world * world_obj;

        // draw the voxel
        hmap.DrawHMap(
                HMAP_DISPW,
                HMAP_MIN_H,
                HMAP_MAX_H,
                pRend,
                W,
                H,
                proj_obj );

        // end of the frame (will present)
        app.EndFrame();
    }

    return 0;
}

