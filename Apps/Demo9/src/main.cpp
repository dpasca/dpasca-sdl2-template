//==================================================================
/// main.cpp
///
/// Created by Davide Pasca - 2023/04/27
/// See the file "license.txt" that comes with this project for
/// copyright info.
//==================================================================

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <array>
#include <vector>
#include <algorithm>
#include "IncludeGL.h"
#include "DBase.h"
#include "MathBase.h"
#include "ImmGL.h"
#include "MinimalSDLApp.h"

//==================================================================
static constexpr float DISP_CAM_NEAR    = 0.1f;     // near plane (meters)
static constexpr float DISP_CAM_FAR     = 1000.f;   // far plane (meters)

struct DemoParams
{
    float       DISP_CAM_FOV_DEG    = 50.f;       // field of view
    float       DISP_CAM_DIST       = 8;
    float       DISP_CAM_HEIGHT     = 4;
    Float2      DISP_CAM_PY_ANGS    = {20.f, 0.f};
};

static DemoParams   _sPar;

static bool _sShowDebugDraw = false;

//==================================================================
inline constexpr float DEG2RAD( float deg )
{
    return glm::pi<float>() / 180.f * deg;
}

// road params
static constexpr auto SLAB_DEPTH = 1.f; // meters
static constexpr auto SLAB_WIDTH = 12.f; // meters

// vehicle params
static constexpr auto VH_MAX_SPEED_MS   = 10.f; // meters/second
static constexpr auto VH_MAX_ACCEL_MS   =  5.f; // meters/second^2
static constexpr auto VH_CRAWL_SPEED_MS = 1.0f; // meters/second
static constexpr auto VH_WIDTH          = 1.0f; // meters
static constexpr auto VH_LENGTH         = 2.0f; // meters
static constexpr auto VH_ELEVATION      = 0.5f; // meters
static constexpr auto VH_YAW_MAX_RAD    = DEG2RAD(30.f); // radians

#ifdef ENABLE_IMGUI
//==================================================================
static void handleUI(size_t frameCnt, auto &immgl)
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
        ImGui::NewLine();
        ImGui::Checkbox("Debug Draw", &_sShowDebugDraw);
    }
}
#endif

//==================================================================
inline void debugDraw( auto &immgl, const Matrix44 &proj_obj )
{
#if 0
    immgl.SetMtxPS( proj_obj );
#endif
}

//==================================================================
static void drawRoad(
        ImmGL& immgl,
        float offZ,
        size_t idxSta,
        size_t idxEnd)
{
    // two levels of gray for asphalt, float values
    static IColor4 baseCols[2] = {
        { 0.4f, 0.4f, 0.4f, 1.f },
        { 0.5f, 0.5f, 0.5f, 1.f },
    };

    for (size_t idx=idxSta; idx < idxEnd; ++idx)
    {
        const auto x0 = -SLAB_WIDTH * 0.5f;
        const auto x1 =  SLAB_WIDTH * 0.5f;
        const auto z0 = (float)(idx  ) * -SLAB_DEPTH;
        const auto z1 = (float)(idx+1) * -SLAB_DEPTH;

        const std::array<IFloat3,4> vpos = {
            IFloat3{x0, 0.f, z0},
            IFloat3{x1, 0.f, z0},
            IFloat3{x0, 0.f, z1},
            IFloat3{x1, 0.f, z1},
        };

        immgl.DrawQuad( vpos, baseCols[idx % std::size(baseCols)] );
    }
}

//==================================================================
static auto attenuateVal = [](auto val, auto dt, auto att)
{
    return val * (decltype(dt))(1 - att * dt);
};

//==================================================================
class Vehicle
{
public:
    // inputs
    float       mIn_AccPedal = 0;
    float       mIn_SteerSUnit = 0;
    // state
    Float3      mPos {0,0,0};
    float       mSpeed = 0;
    float       mAccel = 0;
    float       mYawAng = 0;

    //
    void ApplyInputs(float dt)
    {
        // apply the inputs
        mAccel  += mIn_AccPedal   * VH_MAX_ACCEL_MS * dt;
        mYawAng += mIn_SteerSUnit * VH_YAW_MAX_RAD * dt;
    }

    void AnimateVehicle(float dt)
    {
        mSpeed += mAccel * dt;

        // clamp values
        mSpeed = std::clamp(mSpeed, 0.f, VH_MAX_SPEED_MS);
        mAccel = std::clamp(mAccel, 0.f, VH_MAX_ACCEL_MS);
        mYawAng = std::clamp(mYawAng, -VH_YAW_MAX_RAD, VH_YAW_MAX_RAD);

        // build the velocity vector
        const auto vel = Float3{
            mSpeed * sinf(mYawAng),
            0,
            mSpeed * cosf(mYawAng)
        };

        // apply the velocity to the position
        mPos += vel * dt;

        // wind, friction, etc.
        mSpeed = attenuateVal(mSpeed, dt, 0.1f);

        handleCollisions();
    }

    void DrawVehicle(ImmGL& immgl)
    {
        const auto x0 = mPos[0] - VH_WIDTH  * 0.5f;
        const auto x1 = mPos[0] + VH_WIDTH  * 0.5f;
        const auto z0 = mPos[2] - VH_LENGTH * 0.5f;
        const auto z1 = mPos[2] + VH_LENGTH * 0.5f;

        const std::array<IFloat3,4> vpos = {
            IFloat3{x0, mPos[1], z0},
            IFloat3{x1, mPos[1], z0},
            IFloat3{x0, mPos[1], z1},
            IFloat3{x1, mPos[1], z1},
        };
        const auto baseCol = IColor4{1.0f,0.0f,0.0f,1.f};
        const auto frontCol = IColor4{
            baseCol[0] * 0.7f,
            baseCol[1] * 0.7f,
            baseCol[2] * 0.7f,
            baseCol[3]};
        const auto backCol = IColor4{
            baseCol[0] * 0.9f,
            baseCol[1] * 0.9f,
            baseCol[2] * 0.9f,
            baseCol[3]};

        const std::array<IColor4,4> cols = {
            frontCol,
            frontCol,
            backCol,
            backCol,
        };

        immgl.DrawQuad(vpos, cols);
    }

private:
    //
    void handleCollisions()
    {
        const auto edgeL = -SLAB_WIDTH * (0.5f + 0.05f);
        const auto edgeR =  SLAB_WIDTH * (0.5f + 0.05f);

        if ( mPos[0] < edgeL )
        {
            mPos[0] = edgeL;
            mSpeed = std::min(mSpeed, VH_CRAWL_SPEED_MS);
        }
        else
        if ( mPos[0] > edgeR )
        {
            mPos[0] = edgeR;
            mSpeed = std::min(mSpeed, VH_CRAWL_SPEED_MS);
        }
    }
};

//==================================================================
int main( int argc, char *argv[] )
{
    MinimalSDLApp app( argc, argv, 1024, 768, 0
                    | MinimalSDLApp::FLAG_OPENGL
                    | MinimalSDLApp::FLAG_RESIZABLE
                    );

    ImmGL immgl;

    std::vector<Vehicle> vehicles;
    {
        Vehicle vh;
        vh.mPos[0] = 0;
        vh.mPos[1] = VH_ELEVATION;
        vh.mPos[2] = -4 * SLAB_DEPTH;
        vh.mSpeed = 0;
        vh.mAccel = 0;
        vh.mYawAng = 0;
        vehicles.push_back(vh);
    }

    static constexpr auto FRAME_DT = 1.f / 60.f;

    // begin the main/rendering loop
    for (size_t frameCnt=0; ; ++frameCnt)
    {
        // begin the frame (or get out)
        if ( !app.BeginFrame() )
            break;

#ifdef ENABLE_IMGUI
        app.DrawMainUIWin( [&]() { handleUI(frameCnt, immgl); } );
#endif
        glViewport(0, 0, app.GetDispSize()[0], app.GetDispSize()[1]);
        glClearColor( 0, 0, 0, 0 );
        glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
        glEnable( GL_DEPTH_TEST );

        immgl.ResetStates();

        // obj -> world matrix
        auto world_obj = Matrix44( 1.f );

        // camera -> world matrix
        auto cam_world = [&]()
        {
            auto m = Matrix44( 1.f );
            m = glm::translate( m, Float3(0.0f, -_sPar.DISP_CAM_HEIGHT, -_sPar.DISP_CAM_DIST) );
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

        // set the obj -> proj matrix
        immgl.SetMtxPS( proj_obj );

        // draw the road
        drawRoad(immgl, 0.f, 0, 1000);

        // draw the vehicles
        for (auto& vh : vehicles)
        {
            vh.ApplyInputs(FRAME_DT);
            vh.AnimateVehicle(FRAME_DT);
            vh.DrawVehicle(immgl);
        }

        // draw the debug stuff
        if (_sShowDebugDraw)
            debugDraw( immgl, proj_obj );

        immgl.FlushStdList();

        // end of the frame (will present)
        app.EndFrame();
    }

    return 0;
}

