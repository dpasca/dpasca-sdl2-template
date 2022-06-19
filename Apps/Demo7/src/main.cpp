//==================================================================
/// main.cpp
///
/// Created by Davide Pasca - 2022/06/12
/// See the file "license.txt" that comes with this project for
/// copyright info.
//==================================================================

#define USE_OGL

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <array>
#include <vector>
#include <algorithm> // for std::sort
#include "IncludeGL.h"
#include "DBase.h"
#include "MathBase.h"
#include "RendBase.h"
#include "Plasma2.h"
#include "Terrain.h"
#include "TerrainGen.h"
#include "TerrainExport.h"
#include "MU_WrapMap.h"
#ifdef USE_OGL
# include "ImmGL.h"
#endif
#include "MinimalSDLApp.h"

//==================================================================
static constexpr float DISP_TERR_SCALE  = 10.f;

static constexpr float DISP_CAM_NEAR    = 0.01f;    // near plane (1 cm)
static constexpr float DISP_CAM_FAR     = 1000.f;   // far plane (1000 m)

struct DemoParams
{
    float       DISP_CAM_FOV_DEG    = 65.f;       // field of view
    float       DISP_CAM_DIST       = DISP_TERR_SCALE; // distance from center
    Float2      DISP_CAM_PY_ANGS    = {20.f, 0.f};
    bool        DISP_ANIM_YAW       = true;
    uint32_t    DISP_CROP_WH[2]     = {0,0};

    float       GEN_MIN_H           = -0.15f;
    float       GEN_MAX_H           =  0.10f;
    uint32_t    GEN_SIZL2           = 7;       // 128 x 128 map
    uint32_t    GEN_STASIZL2        = 2;       // 4 x 4 initial random samples
    uint32_t    GEN_SEED            = 100;     // random seed
    float       GEN_ROUGH           = 0.5f;
    bool        GEN_WRAP_EDGES      = false;

    bool        LIGHT_ENABLE_DIFF   = true;
    bool        LIGHT_ENABLE_SHA    = true;
    Float3      LIGHT_DIFF_COL      = {1.0f, 1.0f, 1.0f};
    Float3      LIGHT_AMB_COL       = {0.3f, 0.3f, 0.3f};
    Float2      LIGHT_DIR_LAT_LONG  = {20.f, 70.f};

    std::string EXP_PATHFNAME       {"exported_terr.h"};
    int         EXP_QUANT_HEIGHT    {46};
    int         EXP_QUANT_SHADE     {8};
};

static DemoParams   _sPar;

static int _sForceDebugRendCnt = 0;

//==================================================================
inline float DEG2RAD( float deg )
{
    return glm::pi<float>() / 180.f * deg;
}

//==================================================================
inline Float3 calcLightDir( const Float2 &the_phi_deg )
{
    return glm::euclidean( Float2( DEG2RAD(the_phi_deg[0]), DEG2RAD(the_phi_deg[1]) ) );
}

//==================================================================
//=== Rendering
//==================================================================
// vertex coming from the object
struct VertObj
{
    Float3      pos;
    float       siz;
    RBColType   col;
};
// vertex output in screen space
struct VertDev
{
    Float3      pos {};
    Float2      siz {};
    RBColType   col {};
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

#ifdef USE_OGL
    vdev.pos[0] = posH[0] * oow;
    vdev.pos[1] = posH[1] * oow;
#else
    vdev.pos[0] = deviceW * (posH[0] * oow + 1) * 0.5f;
    vdev.pos[1] = deviceH * (1 - posH[1] * oow) * 0.5f;
#endif
    vdev.pos[2] = posH[2] * oow;

#ifdef USE_OGL
    vdev.siz[0] = vobj.siz * oow;
    vdev.siz[1] = vobj.siz * oow;
#else
    vdev.siz[0] = deviceW * vobj.siz * oow;
    vdev.siz[1] = deviceH * vobj.siz * oow;
#endif
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
    c_auto w = vdev.siz[0];
    c_auto h = vdev.siz[1];
    c_auto x = (float)(vdev.pos[0] - w*0.5f);
    c_auto y = (float)(vdev.pos[1] - h*0.5f);
#ifdef USE_OGL
    pRend->DrawRectFill( x, y, vdev.pos[2], w, h,
            IColor4({
                vdev.col[0] * (1.f/255),
                vdev.col[1] * (1.f/255),
                vdev.col[2] * (1.f/255),
                vdev.col[3] * (1.f/255) }) );
#else
    c_auto c = vdev.col;
    SDL_SetRenderDrawColor( pRend, c[0], c[1], c[2], c[3] );
    SDL_FRect rc;
    rc.x = x;
    rc.y = y;
    rc.w = w;
    rc.h = h;
    SDL_RenderFillRectF( pRend, &rc );
#endif
}

//==================================================================
static void drawTerrain(
        auto &terr,
        float dispSca,
        const uint32_t cropWH[2],
        auto *pRend,
        float deviceW,
        float deviceH,
        const Matrix44 &proj_obj )
{
    c_auto siz = terr.GetSiz();

    c_auto dxdt = dispSca / (float)siz;

    c_auto cropRC = TERR_MakeCropRC( terr.GetSiz(), cropWH );
    c_auto xi1 = cropRC[0];
    c_auto yi1 = cropRC[1];
    c_auto xi2 = cropRC[2];
    c_auto yi2 = cropRC[3];

    std::vector<VertDev> vertsDev;
    vertsDev.reserve( (yi2 - yi1) * (xi2 - xi1) );

    c_auto oosiz = 1.f / siz;
    for (size_t yi=yi1; yi < yi2; ++yi)
    {
        c_auto y = glm::mix( -0.5f, 0.5f, yi * oosiz );

        c_auto rowCellIdx = yi << terr.GetSizL2();
        for (size_t xi=xi1; xi < xi2; ++xi)
        {
            c_auto x = glm::mix( -0.5f, 0.5f, xi * oosiz );

            c_auto cellIdx = xi + rowCellIdx;

            VertObj vobj;
            vobj.pos = dispSca * Float3( x, terr.mHeights[ cellIdx ], y );
            vobj.siz = dxdt;
            vobj.col = terr.mBakedCols[ cellIdx ];

            // convert from object-space to device-space (2D display dimensions)
            c_auto vout = makeDeviceVert( proj_obj, vobj, deviceW, deviceH );

            // store the vertex
            if ( vout.pos[2] > 0 )
                vertsDev.push_back( vout );
        }
    }

#ifndef USE_OGL
    // sort with bigger Z first
    std::sort( vertsDev.begin(), vertsDev.end(), []( c_auto &l, c_auto &r )
    {
        return l.pos[2] > r.pos[2];
    });
#endif
    // finally render the verts
    for (c_auto &v : vertsDev)
        drawAtom( pRend, v );
}

//==================================================================
//=== Generation
//==================================================================
static void makeTerrFromParams( auto &terr )
{
    // allocate a new map
    terr = Terrain( _sPar.GEN_SIZL2 );

    // fill it with "plasma"
    Plasma2::Params par;
    par.pDest       = terr.mHeights.data(); // destination values
    par.sizL2       = terr.GetSizL2();      // log2 of size (i.e. 7 = 128 pixels width/height)
    par.baseSizL2   = _sPar.GEN_STASIZL2;// log2 of size of initial low res map
    par.seed        = _sPar.GEN_SEED;
    par.rough       = _sPar.GEN_ROUGH;

    // generate the map
    Plasma2 plasma( par );
    while ( plasma.IterateRow() )
    {
    }

    // transform heights to the required range
    TGEN_ScaleHeights( terr, _sPar.GEN_MIN_H, _sPar.GEN_MAX_H );

    // blend edges to make a continuous map
    if ( _sPar.GEN_WRAP_EDGES )
    {
        // we wrap by cross-blending the extreme 1/3 of the samples at the edges
        c_auto wrapSiz = terr.GetSiz() / 3;
        MU_WrapMap<float,1>( terr.mHeights.data(), terr.GetSizL2(), wrapSiz );
    }

    TGEN_MakeMateAndTex( terr );

    TGEN_FlattenSeaBed( terr );

    if ( _sPar.LIGHT_ENABLE_DIFF )
        TGEN_CalcDiffLight( terr, calcLightDir( _sPar.LIGHT_DIR_LAT_LONG ) );

    if ( _sPar.LIGHT_ENABLE_SHA )
        TGEN_CalcShadows( terr, calcLightDir( _sPar.LIGHT_DIR_LAT_LONG ) );

    TGEN_CalcBakedColors( terr, _sPar.LIGHT_DIFF_COL, _sPar.LIGHT_AMB_COL );
}

#ifdef ENABLE_IMGUI
//==================================================================
static void handleUI( size_t frameCnt, Terrain &terr )
{
    //ImGui::Text( "Frame: %zu", frameCnt );

    auto header = []( const std::string &name, bool defOpen )
    {
        return ImGui::CollapsingHeader(
                    (name + "##head").c_str(),
                    defOpen ? ImGuiTreeNodeFlags_DefaultOpen : 0);
    };

    if ( header( "Display", true ) )
    {
        ImGui::SliderFloat( "Camera FOV", &_sPar.DISP_CAM_FOV_DEG, 10.f, 120.f );
        ImGui::SliderFloat( "Camera Dist", &_sPar.DISP_CAM_DIST, 0.f, DISP_CAM_FAR/10 );
        ImGui::SliderFloat2( "Camera Pitch/Yaw", &_sPar.DISP_CAM_PY_ANGS[0], -180, 180 );

        ImGui::Checkbox( "Anim Yaw", &_sPar.DISP_ANIM_YAW );

        ImGui::InputScalarN( "Crop", ImGuiDataType_U32, _sPar.DISP_CROP_WH, 2 );
    }

    bool rebuild = false;

    if ( header( "Generation", true ) )
    {
        auto inputU32 = []( c_auto *pName, uint32_t *pVal, uint32_t step )
        {
            return ImGui::InputScalar( pName, ImGuiDataType_U32, pVal, &step, nullptr, "%d" );
        };
        auto slideU32 = []( c_auto *pName, uint32_t *pVal, uint32_t mi, uint32_t ma )
        {
            return ImGui::SliderScalar( pName, ImGuiDataType_U32, pVal, &mi, &ma, nullptr, 0 );
        };

        rebuild |= ImGui::InputFloat( "Max Height", &_sPar.GEN_MAX_H, 0.01f, 0.1f );
        rebuild |= ImGui::InputFloat( "Min Height", &_sPar.GEN_MIN_H, 0.01f, 0.1f );
        rebuild |= slideU32( "Size Log2", &_sPar.GEN_SIZL2, 0, 9 );
        rebuild |= slideU32( "Init Size Log2", &_sPar.GEN_STASIZL2, 0, _sPar.GEN_SIZL2 );
        rebuild |= ImGui::InputFloat( "Roughness", &_sPar.GEN_ROUGH, 0.01f, 0.1f );
        rebuild |= inputU32( "Seed", &_sPar.GEN_SEED, 1 );
        rebuild |= ImGui::Checkbox( "Wrap Edges", &_sPar.GEN_WRAP_EDGES );
    }

    if ( header( "Lighting", false ) )
    {
        rebuild |= ImGui::Checkbox( "Enable Diffuse", &_sPar.LIGHT_ENABLE_DIFF );
        rebuild |= ImGui::Checkbox( "Enable Shadows", &_sPar.LIGHT_ENABLE_SHA );

        auto inputF3 = []( c_auto *pName, Float3 &val, float mi, float ma )
        {
            c_auto changed = ImGui::InputFloat3( pName, &val[0] );

            val = glm::clamp( val, Float3{ mi, mi, mi }, Float3{ ma, ma, ma } );
            return changed;
        };

        bool moved {};
        auto &ll = _sPar.LIGHT_DIR_LAT_LONG;
        moved |= ImGui::SliderFloat( "Light Dir Latitude",  &ll[0], 0,  90 );
        moved |= ImGui::SliderFloat( "Light Dir Longitude", &ll[1], 0, 360 );
        rebuild |= moved;

        if ( moved )
            _sForceDebugRendCnt = 60*4;

        rebuild |= inputF3( "Light Col", _sPar.LIGHT_DIFF_COL, 0, 5 );
        rebuild |= inputF3( "Ambient Col", _sPar.LIGHT_AMB_COL, 0, 5 );
    }

    if ( rebuild )
    {
        _sPar.GEN_STASIZL2 = std::min( _sPar.GEN_STASIZL2, _sPar.GEN_SIZL2 );
        _sPar.GEN_ROUGH    = std::clamp( _sPar.GEN_ROUGH, 0.f, 1.f );

        makeTerrFromParams( terr );
    }

    if ( header( "Export", false ) )
    {
        ImGui::InputText( "Login Name", &_sPar.EXP_PATHFNAME );
        ImGui::InputInt( "Height levels", &_sPar.EXP_QUANT_HEIGHT );
        ImGui::InputInt( "Shade Levels", &_sPar.EXP_QUANT_SHADE );

        if ( ImGui::Button( "Export" ) )
            TerrainExport(
                    terr,
                    _sPar.EXP_PATHFNAME,
                    _sPar.EXP_QUANT_HEIGHT,
                    _sPar.EXP_QUANT_SHADE,
                    _sPar.DISP_CROP_WH );
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
#ifdef USE_OGL
#else
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

    draw3DLine(
            Float3(0,0,0),
            calcLightDir( _sPar.LIGHT_DIR_LAT_LONG ) * DISP_TERR_SCALE * 0.5f );
#endif
}

//==================================================================
int main( int argc, char *argv[] )
{
    MinimalSDLApp app( argc, argv, 1024, 768, 0
#ifdef USE_OGL
                    | MinimalSDLApp::FLAG_OPENGL
#endif
                    | MinimalSDLApp::FLAG_RESIZABLE
                    );
#ifdef USE_OGL
    ImmGL immgl;
#endif

    Terrain terr;
    makeTerrFromParams( terr );

    // begin the main/rendering loop
    for (size_t frameCnt=0; ; ++frameCnt)
    {
        // begin the frame (or get out)
        if ( !app.BeginFrame() )
            break;

#ifdef ENABLE_IMGUI
        app.DrawMainUIWin( [&]() { handleUI( frameCnt, terr ); } );
#endif
#ifdef USE_OGL
        glViewport(0, 0, app.GetDispSize()[0], app.GetDispSize()[1]);
        glClearColor( 0, 0, 0, 0 );
        glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );

        glEnable( GL_DEPTH_TEST );

        auto *pRend = &immgl;
        pRend->ResetStates();
#else
        // get the renderer
        auto *pRend = app.GetRenderer();

        SDL_SetRenderDrawColor( pRend, 0, 0, 0, 0 );
        SDL_RenderClear( pRend );
#endif
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

        c_auto [curW, curH] = app.GetDispSize();

        // camera -> projection matrix
        c_auto proj_camera = glm::perspective(
                                DEG2RAD( _sPar.DISP_CAM_FOV_DEG ),  // FOV
                                (float)curW / curH,                       // aspect ratio
                                DISP_CAM_NEAR,
                                DISP_CAM_FAR );

        // obj -> proj matrix
        c_auto proj_obj = proj_camera * cam_world * world_obj;

        // draw the terrain
        drawTerrain(
                terr,
                DISP_TERR_SCALE,
                _sPar.DISP_CROP_WH,
                pRend,
                (float)curW,
                (float)curH,
                proj_obj );

        //
        if ( _sForceDebugRendCnt )
        {
            _sForceDebugRendCnt -= 1;
            debugDraw( pRend, (float)curW, (float)curH, proj_obj );
        }

#ifdef USE_OGL
        pRend->FlushPrims();
#endif
        // end of the frame (will present)
        app.EndFrame();
    }

    return 0;
}

