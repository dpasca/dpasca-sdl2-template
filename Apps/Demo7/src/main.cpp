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
#include "IncludeGL.h"
#include "DBase.h"
#include "MathBase.h"
#include "RendBase.h"
#include "Plasma2.h"
#include "Terrain.h"
#include "TerrainGen.h"
#include "TerrainExport.h"
#include "MU_WrapMap.h"
#include "ImmGL.h"
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
    bool        DISP_SMOOTH         = false;

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
static std::vector<Float3> makeTerrVerts( auto &terr, float sca )
{
    c_auto siz = terr.GetSiz();

    std::vector<Float3> verts( siz * siz );

    c_auto oosiz = 1.f / siz;
    size_t idx = 0;
    for (size_t yi=0; yi < siz; ++yi)
    {
        c_auto y = glm::mix( -0.5f, 0.5f, yi * oosiz );
        for (size_t xi=0; xi < siz; ++xi, ++idx)
        {
            c_auto x = glm::mix( -0.5f, 0.5f, xi * oosiz );
            verts[ idx ] = { sca * Float3( x, terr.mHeights[ idx ], y ) };
        }
    }

    return verts;
}

//==================================================================
static auto makeRendCol = []( RBColType src )
{
    return IColor4((float)src[0],(float)src[1],(float)src[2],(float)src[3]) * (1.f/255);
};

//==================================================================
static void makeTerrGeometry(
        auto &immgl,
        auto &oList,
        const auto &terr,
        const uint32_t cropWH[2],
        const bool smoothShade )
{
    c_auto terrVerts = makeTerrVerts( terr, DISP_TERR_SCALE );

    c_auto cropRC = TERR_MakeCropRC( terr.GetSiz(), cropWH );
    c_auto xi1 = cropRC[0];
    c_auto yi1 = cropRC[1];
    c_auto xi2 = cropRC[2] - 1;
    c_auto yi2 = cropRC[3] - 1;

    oList = immgl.NewList( [&]( ImmGLList &lst )
    {
        if ( smoothShade )
        {
            // verts
            {
                auto *pPos = lst.AllocPos( terrVerts.size() );
                auto *pCol = lst.AllocCol( terrVerts.size() );
                for (size_t i=0; i < terrVerts.size(); ++i)
                {
                    pPos[i] = terrVerts[i];
                    pCol[i] = makeRendCol( terr.mBakedCols[ i ] );
                }
            }
            // indexes
            for (size_t yi=yi1; yi < yi2; ++yi)
            {
                c_auto row0 = (yi+0) << terr.GetSizL2();
                c_auto row1 = (yi+1) << terr.GetSizL2();
                for (size_t xi=xi1; xi < xi2; ++xi)
                {
                    c_auto i00 = (uint32_t)(row0 + xi+0);
                    c_auto i01 = (uint32_t)(row0 + xi+1);
                    c_auto i10 = (uint32_t)(row1 + xi+0);
                    c_auto i11 = (uint32_t)(row1 + xi+1);
                    ImmGL_MakeQuadOfTrigs( lst.AllocIdx( 6 ), i00, i01, i10, i11 );
                }
            }
        }
        else
        {
            for (size_t yi=yi1; yi < yi2; ++yi)
            {
                c_auto row0 = (yi+0) << terr.GetSizL2();
                c_auto row1 = (yi+1) << terr.GetSizL2();
                for (size_t xi=xi1; xi < xi2; ++xi)
                {
                    c_auto i00 = row0 + xi+0;
                    c_auto i01 = row0 + xi+1;
                    c_auto i10 = row1 + xi+0;
                    c_auto i11 = row1 + xi+1;
                    auto *pPos = lst.AllocPos( 6 );
                    auto *pCol = lst.AllocCol( 6 );
                    ImmGL_MakeQuadOfTrigs( pPos,
                        terrVerts[ i00 ], terrVerts[ i01 ],
                        terrVerts[ i10 ], terrVerts[ i11 ] );

                    c_auto col = makeRendCol( terr.mBakedCols[ i00 ] );
                    ImmGL_MakeQuadOfTrigs( pCol, col, col, col, col );
                }
            }
        }
    });
}

//==================================================================
//=== Generation
//==================================================================
static void makeTerrFromParams( auto &immgl, ImmGLListPtr &oList, auto &terr )
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

    //
    oList = {};
    makeTerrGeometry( immgl, oList, terr, _sPar.DISP_CROP_WH, _sPar.DISP_SMOOTH );
}

#ifdef ENABLE_IMGUI
//==================================================================
static void handleUI(
            size_t frameCnt,
            auto &immgl,
            auto &oList,
            Terrain &terr )
{
    //ImGui::Text( "Frame: %zu", frameCnt );

    auto header = []( const std::string &name, bool defOpen )
    {
        return ImGui::CollapsingHeader(
                    (name + "##head").c_str(),
                    defOpen ? ImGuiTreeNodeFlags_DefaultOpen : 0);
    };

    bool rebuild = false;

    if ( header( "Display", true ) )
    {
        ImGui::SliderFloat( "Camera FOV", &_sPar.DISP_CAM_FOV_DEG, 10.f, 120.f );
        ImGui::SliderFloat( "Camera Dist", &_sPar.DISP_CAM_DIST, 0.f, DISP_CAM_FAR/10 );
        ImGui::SliderFloat2( "Camera Pitch/Yaw", &_sPar.DISP_CAM_PY_ANGS[0], -180, 180 );

        ImGui::Checkbox( "Anim Yaw", &_sPar.DISP_ANIM_YAW );

        rebuild |= ImGui::InputScalarN( "Crop", ImGuiDataType_U32, _sPar.DISP_CROP_WH, 2 );
        rebuild |= ImGui::Checkbox( "Smooth Shading", &_sPar.DISP_SMOOTH );
    }

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
            _sForceDebugRendCnt = 60*6;

        rebuild |= inputF3( "Light Col", _sPar.LIGHT_DIFF_COL, 0, 5 );
        rebuild |= inputF3( "Ambient Col", _sPar.LIGHT_AMB_COL, 0, 5 );
    }

    if ( rebuild )
    {
        _sPar.GEN_STASIZL2 = std::min( _sPar.GEN_STASIZL2, _sPar.GEN_SIZL2 );
        _sPar.GEN_ROUGH    = std::clamp( _sPar.GEN_ROUGH, 0.f, 1.f );

        makeTerrFromParams( immgl, oList, terr );
    }

    if ( header( "Export", false ) )
    {
        ImGui::InputText( "Login Name", &_sPar.EXP_PATHFNAME );
        ImGui::InputInt( "Height levels", &_sPar.EXP_QUANT_HEIGHT );
        ImGui::InputInt( "Shade Levels", &_sPar.EXP_QUANT_SHADE );

        if ( ImGui::Button( "Export" ) )
        {
            std::string headStr;
            auto addField = [&]( const std::string &name, c_auto &val )
            {
                headStr += "// " + name + " = " + std::to_string(val) + "\n";
            };
            addField( "Gen:: Max Height",     _sPar.GEN_MIN_H      );
            addField( "Gen:: Min Height",     _sPar.GEN_MAX_H      );
            addField( "Gen:: Size Log2",      _sPar.GEN_SIZL2      );
            addField( "Gen:: Init Size Log2", _sPar.GEN_STASIZL2   );
            addField( "Gen:: Seed",           _sPar.GEN_SEED       );
            addField( "Gen:: Roughness",      _sPar.GEN_ROUGH      );
            addField( "Gen:: Wrap Edges",     _sPar.GEN_WRAP_EDGES );

            TerrainExport(
                    terr,
                    _sPar.EXP_PATHFNAME,
                    headStr,
                    _sPar.EXP_QUANT_HEIGHT,
                    _sPar.EXP_QUANT_SHADE,
                    _sPar.DISP_CROP_WH );
        }
    }
}
#endif

//==================================================================
inline void debugDraw( auto &immgl, const Matrix44 &proj_obj )
{
    immgl.SetMtxPS( proj_obj );

    auto sca = DISP_TERR_SCALE * 0.5f;

    c_auto dir = calcLightDir( _sPar.LIGHT_DIR_LAT_LONG );

    immgl.DrawLine( {0,0,0}, dir * Float3(sca,sca,sca), {0.f,1.f,0.f,0.5f} );
    immgl.DrawLine( {0,0,0}, dir * Float3(sca,  0,sca), {0.f,0.f,0.f,1.0f} );
}

//==================================================================
int main( int argc, char *argv[] )
{
    MinimalSDLApp app( argc, argv, 1024, 768, 0
                    | MinimalSDLApp::FLAG_OPENGL
                    | MinimalSDLApp::FLAG_RESIZABLE
                    );

    ImmGL immgl;
    ImmGLListPtr oList;

    Terrain terr;
    makeTerrFromParams( immgl, oList, terr );

    // begin the main/rendering loop
    for (size_t frameCnt=0; ; ++frameCnt)
    {
        // begin the frame (or get out)
        if ( !app.BeginFrame() )
            break;

#ifdef ENABLE_IMGUI
        app.DrawMainUIWin( [&]() { handleUI( frameCnt, immgl, oList, terr ); } );
#endif
        glViewport(0, 0, app.GetDispSize()[0], app.GetDispSize()[1]);
        glClearColor( 0, 0, 0, 0 );
        glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
        glEnable( GL_DEPTH_TEST );

        immgl.ResetStates();

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

        // draw the terrain by calling the display list
        immgl.SetMtxPS( proj_obj );
        immgl.CallList( *oList );

        //
        if ( _sForceDebugRendCnt )
        {
            _sForceDebugRendCnt -= 1;
            debugDraw( immgl, proj_obj );
        }

        immgl.FlushStdList();

        // end of the frame (will present)
        app.EndFrame();
    }

    return 0;
}

