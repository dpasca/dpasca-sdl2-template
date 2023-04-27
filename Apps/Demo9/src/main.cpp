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
#include <random>
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

static bool _sShowDebugDraw = true;

//==================================================================
static constexpr auto PI2 = 2*glm::pi<float>();

inline constexpr float DEG2RAD( float deg )
{
    return glm::pi<float>() / 180.f * deg;
}

// road params
static constexpr auto SLAB_DEPTH = 1.f; // meters
static constexpr auto SLAB_WIDTH = 12.f; // meters
static constexpr auto SLAB_MAX_N = 1000; // meters
static constexpr auto SLAB_LANES_N = 4;

// vehicle params
static constexpr auto VH_MAX_SPEED_MS   = 20.f; // meters/second
static constexpr auto VH_MAX_ACCEL_MS   =  5.f; // meters/second^2
static constexpr auto VH_CRAWL_SPEED_MS = 1.0f; // meters/second
static constexpr auto VH_WIDTH          = 1.0f; // meters
static constexpr auto VH_LENGTH         = 2.0f; // meters
static constexpr auto VH_ELEVATION      = 0.5f; // meters
static constexpr auto VH_YAW_MAX_RAD    = DEG2RAD(30.f); // radians
static constexpr auto VH_PROBES_N       = 16;
static constexpr auto VH_PROBE_RADIUS   = VH_LENGTH * 3;

static constexpr auto NPC_SPAWN_N       = (size_t)150;
static constexpr auto NPC_SPEED_MIN_MS  = 4.0f; // meters/second
static constexpr auto NPC_SPEED_MAX_MS  = 8.0f; // meters/second

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
    // probe distances to targets
    float       mIn_ProbeUnitDist[VH_PROBES_N] = {0};
    // probe velocities of target
    float       mIn_ProbeVelXs[VH_PROBES_N] = {0};
    float       mIn_ProbeVelZs[VH_PROBES_N] = {0};

    // state
    Float3      mPos {0,0,0};
    float       mSpeed = 0;
    float       mAccel = 0;
    float       mYawAng = 0;

    bool        mIsNPC = true;

    //
    void ApplyInputs(float dt)
    {
        // apply the inputs
        mAccel  += mIn_AccPedal   * VH_MAX_ACCEL_MS * dt;
        mYawAng += mIn_SteerSUnit * VH_YAW_MAX_RAD * dt;
    }

    void AnimateVehicle(float dt)
    {
        // NPCs just go forward
        if (mIsNPC)
        {
            mPos[2] += -mSpeed * dt;
            handleWrapping();
            return;
        }

        mSpeed += mAccel * dt;

        // clamp values
        mSpeed = std::clamp(mSpeed, 0.f, VH_MAX_SPEED_MS);
        mAccel = std::clamp(mAccel, 0.f, VH_MAX_ACCEL_MS);
        mYawAng = std::clamp(mYawAng, -VH_YAW_MAX_RAD, VH_YAW_MAX_RAD);

        // build the velocity vector
        const auto vel = Float3{
            -mSpeed * sinf(mYawAng),
            0,
            -mSpeed * cosf(mYawAng)
        };

        // apply the velocity to the position
        mPos += vel * dt;

        // wind, friction, etc.
        mSpeed = attenuateVal(mSpeed, dt, 0.1f);

        handleWrapping();
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

        const auto baseCol = mIsNPC
            ? IColor4{0.0f,0.0f,1.0f,1.f}
            : IColor4{1.0f,0.0f,0.0f,1.f};

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
    void handleWrapping()
    {
        const auto trackMinZ = -SLAB_DEPTH * (SLAB_MAX_N - 1);
        if (mPos[2] < trackMinZ)
            mPos[2] -= trackMinZ;
    }
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
template <typename VEC_T>
static double calcYawToTarget(
    const VEC_T& fwd,
    const VEC_T& pos,
    const VEC_T& targetPos)
{
    const auto targetDir = glm::normalize(targetPos - pos);
    //const auto fwdXZ = glm::normalize( VEC_T(fwd[0], 0.0f, fwd[2]) );
    double yaw = atan2(targetDir[2], targetDir[0]) - atan2(fwd[2], fwd[0]);
    yaw = atan2(sin(yaw), cos(yaw));
    return yaw;
}

//==================================================================
static void fillVehicleSensors(Vehicle& vh, const std::vector<Vehicle>& others, size_t skipIdx)
{
    for (size_t i=0; i < VH_PROBES_N; ++i)
    {
        vh.mIn_ProbeUnitDist[i] = 1.0f; // 1 at radius or more
        vh.mIn_ProbeVelXs[i] = 0;
        vh.mIn_ProbeVelZs[i] = 0;
    }

    // arc of a probe
    const auto probeAngLen = PI2 / VH_PROBES_N;

    for (size_t i=0; i < others.size(); ++i)
    {
        if (i == skipIdx)
            continue;

        const auto& other = others[i];

        // get the distance, no sqr optimization 8)
        const auto unitDist = glm::distance(vh.mPos, other.mPos) / VH_PROBE_RADIUS;
        if (unitDist > 1.0f)
            continue;

        // find the yaw to the other vehicle
        const auto yaw = calcYawToTarget(Float3(0,0,-1), vh.mPos, other.mPos);
        // select a sensor index based on the yaw, given VH_PROBES_N distributed
        // around the circle

        // offset the yaw so that we're in the middle of the range of the sensor
        //  (we want the front sensor to grab left and right equally)
#if 0
example with 4 probes
                     0
      -probeAngLen/2 | +probeAngLen/2
                   \ | /
                    \|/
                1----+----3
                    /|\
                   / | \
                     |
                     2

probeAngLen = 2*pi / 4 (90 degrees)
#endif
        // offet into our probe-space
        auto probeYaw = yaw + probeAngLen * 0.5f;
        // wrap around
        if (probeYaw < 0)
            probeYaw += PI2;

        // get the index, and wrap it around
        const auto probeIdx = (size_t)((probeYaw / PI2) * (float)VH_PROBES_N) % VH_PROBES_N;

        // now that we know into which probe does the target fall, see if the distance is
        // less than the current one, and overwrite if so
        if (unitDist < vh.mIn_ProbeUnitDist[probeIdx])
        {
            // build the velocity vector
            const auto vel = Float3{
                -other.mSpeed * sinf(other.mYawAng),
                0,
                -other.mSpeed * cosf(other.mYawAng),
            };

            vh.mIn_ProbeUnitDist[probeIdx] = unitDist;
            vh.mIn_ProbeVelXs[probeIdx] = vel[0];
            vh.mIn_ProbeVelZs[probeIdx] = vel[2];
        }
    }
}

//==================================================================
static IColor4 hueToColor(float hue)
{
    // https://en.wikipedia.org/wiki/HSL_and_HSV#From_HSV
    const auto C = 1.0f;
    const auto X = C * (1.0f - std::abs( std::fmod(hue / 60.0f, 2.0f) - 1.0f ));
    const auto m = 0.0f;

    if (hue >=   0.0f && hue <  60.0f) return {C, X, 0.0f, 1}; else
    if (hue >=  60.0f && hue < 120.0f) return {X, C, 0.0f, 1}; else
    if (hue >= 120.0f && hue < 180.0f) return {0.0f, C, X, 1}; else
    if (hue >= 180.0f && hue < 240.0f) return {0.0f, X, C, 1}; else
    if (hue >= 240.0f && hue < 300.0f) return {X, 0.0f, C, 1}; else
                                       return {C, 0.0f, X, 1};
}

//==================================================================
inline void debugDraw(auto &immgl, const Vehicle& vh)
{
    static constexpr auto PI2 = 2*glm::pi<float>();

    const auto probeAngLen = PI2 / VH_PROBES_N;

    const auto fwdSca = Float3(1,1,-1);

    // draw the vehicle's probes
    for (size_t i=0; i < VH_PROBES_N; ++i)
    {
        // calc probe min and max angle
        const auto probeAngMin = probeAngLen * ((float)i - 0.5f);
        const auto probeAngMax = probeAngMin + probeAngLen;

        const auto probeCol =
            hueToColor(360.f * (float)i / (float)VH_PROBES_N) * IColor4(0.7f, 0.7f, 0.7f, 0.3f);

        const auto drawDist = vh.mIn_ProbeUnitDist[i] * VH_PROBE_RADIUS;

        auto makeRotDist = [&](float ang)
        {
            return Float3{
                drawDist * sinf(ang),
                0,
                drawDist * cosf(ang),
            };
        };

        // slightly above the vehicle
        const auto basePos = vh.mPos + Float3(0, 0.3f, 0);

        const auto probePos    = basePos;
        const auto probePosMin = basePos + fwdSca * makeRotDist(probeAngMin);
        const auto probePosMax = basePos + fwdSca * makeRotDist(probeAngMax);
        immgl.DrawTri({probePos, probePosMin, probePosMax}, probeCol);
    }
}

//==================================================================
int main( int argc, char *argv[] )
{
    MinimalSDLApp app( argc, argv, 1024, 768, 0
                    | MinimalSDLApp::FLAG_OPENGL
                    | MinimalSDLApp::FLAG_RESIZABLE
                    );

    ImmGL immgl;

    std::vector<Vehicle> vehicles;
    // at 0, is our vehicle
    {
        Vehicle vh;
        vh.mPos[0] = 0;
        vh.mPos[1] = VH_ELEVATION;
        vh.mPos[2] = -4 * SLAB_DEPTH;
        vh.mIsNPC = false;
        vehicles.push_back(vh);
    }

    {
        // random gen and distribution
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<float> dist(0.f, 1.f);

        // generate some NPC vehicles
        for (size_t i=0; i < NPC_SPAWN_N; ++i)
        {
            // random x at center of each lane, based on SLAB_LANES_N
            const auto laneW = SLAB_WIDTH / SLAB_LANES_N;
            const auto lane = dist(gen) * (SLAB_LANES_N-1);
            const auto x = lane * laneW - SLAB_WIDTH * 0.5f + laneW * 0.5f;

            // z from slab 0 to SLAB_MAX_N-1
            const auto z = dist(gen) * (SLAB_MAX_N-1) * -SLAB_DEPTH;

            Vehicle vh;
            vh.mPos[0] = x;
            vh.mPos[1] = VH_ELEVATION;
            vh.mPos[2] = z;
            vh.mSpeed = glm::mix(NPC_SPEED_MIN_MS, NPC_SPEED_MAX_MS, dist(gen));
            vh.mIsNPC = true;
            vehicles.push_back(vh);
        }
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
        immgl.SetBlendAlpha();

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
        drawRoad(immgl, 0.f, 0, SLAB_MAX_N);

        // animate the vehicles
        fillVehicleSensors(vehicles[0], vehicles, 0);
        for (auto& vh : vehicles)
        {
            vh.ApplyInputs(FRAME_DT);
            vh.AnimateVehicle(FRAME_DT);
        }

        // draw the vehicles
        for (auto& vh : vehicles)
            vh.DrawVehicle(immgl);

        // draw the debug stuff
        if (_sShowDebugDraw)
            debugDraw(immgl, vehicles[0]);

        immgl.FlushStdList();

        // end of the frame (will present)
        app.EndFrame();
    }

    return 0;
}

