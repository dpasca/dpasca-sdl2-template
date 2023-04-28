//==================================================================
/// Simulation.h
///
/// Created by Davide Pasca - 2023/04/28
/// See the file "license.txt" that comes with this project for
/// copyright info.
//==================================================================

#ifndef SIMULATION_H
#define SIMULATION_H

#include <array>
#include <vector>
#include <algorithm>
#include <random>
#include <cassert>
#include "DBase.h"
#include "MathBase.h"
#include "IncludeGL.h"
#include "ImmGL.h"
#include "CS_BrainBase.h"

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
static constexpr auto SLAB_STA_IDX = 4;
static constexpr auto SLAB_END_IDX = SLAB_MAX_N - 10;
static constexpr auto SLAB_LANES_N = 4;

// vehicle params
static constexpr auto VH_MAX_SPEED_MS   = 20.f; // meters/second
static constexpr auto VH_MAX_ACCEL_MS   = 200.f; // meters/second^2
static constexpr auto VH_CRAWL_SPEED_MS = 1.0f; // meters/second
static constexpr auto VH_WIDTH          = 1.0f; // meters
static constexpr auto VH_LENGTH         = 2.0f; // meters
static constexpr auto VH_ELEVATION      = 0.5f; // meters
static constexpr auto VH_YAW_MAX_RAD    = DEG2RAD(30.f); // radians
static constexpr auto VH_PROBE_RADIUS   = VH_LENGTH * 3;

static constexpr auto NPC_SPAWN_N       = (size_t)150;
static constexpr auto NPC_SPEED_MIN_MS  = 4.0f; // meters/second
static constexpr auto NPC_SPEED_MAX_MS  = 8.0f; // meters/second

static constexpr auto SIM_TRAIN_VARIANTS_N = 5;

//==================================================================
static auto attenuateVal = [](auto val, auto dt, auto att)
{
    return val * (decltype(dt))(1 - att * dt);
};

//==================================================================
class Vehicle
{
public:
    static inline constexpr size_t PROBES_N = 16;

    // we put all of the sensors in a single enum and single array, to easily
    // feed them to the feed-forward NN
    enum Sensors : size_t
    {
        SENS_POS_X,
        SENS_SPEED,
        SENS_YAW,
        SENS_PROBE_FIRST_UNITDIST,
        SENS_PROBE_LAST_UNITDIST = SENS_PROBE_FIRST_UNITDIST + PROBES_N - 1,

        SENS_PROBE_FIRST_VEL_X,
        SENS_PROBE_LAST_VEL_X = SENS_PROBE_FIRST_VEL_X + PROBES_N - 1,

        SENS_PROBE_FIRST_VEL_Z,
        SENS_PROBE_LAST_VEL_Z = SENS_PROBE_FIRST_VEL_Z + PROBES_N - 1,

        SENS_N
    };
    enum Controls : size_t
    {
        CTRL_ACCEL_PEDAL, // 0..1
        CTRL_STEER_UNIT,  // 0..1 (left to right)
        CTRL_N
    };
    // sensors, these are the inputs to the brain
    float       mSens[SENS_N] {0};
    // controls, these are the outputs of the brain
    float       mCtrls[CTRL_N] {0};

    // state
    Float3      mPos {0,0,0};
    float       mSpeed = 0;
    float       mAccel = 0;
    float       mYawAng = 0;

    bool        mIsNPC = true;

    //
    void ApplyControls(float dt)
    {
        // modify the state by applying the controls
        mAccel  += dt * VH_MAX_ACCEL_MS *  mCtrls[CTRL_ACCEL_PEDAL];
        mYawAng += dt * VH_YAW_MAX_RAD  * (mCtrls[CTRL_STEER_UNIT] - 0.5f);
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
        //handleSoftEdges();
        //handleHardEdges();
    }

private:
    void handleWrapping()
    {
        const auto trackMinZ = -SLAB_DEPTH * (SLAB_MAX_N - 1);
        if (mPos[2] < trackMinZ)
            mPos[2] -= trackMinZ;
    }
    // these are soft-edges... slow down when going to gravel
    void handleSoftEdges()
    {
        const auto edgeL = -SLAB_WIDTH * (0.5f - 0.01f);
        const auto edgeR =  SLAB_WIDTH * (0.5f - 0.01f);

        if ( mPos[0] < edgeL )
        {
            //mPos[0] = edgeL;
            mSpeed = std::min(mSpeed, VH_CRAWL_SPEED_MS);
        }
        else
        if ( mPos[0] > edgeR )
        {
            //mPos[0] = edgeR;
            mSpeed = std::min(mSpeed, VH_CRAWL_SPEED_MS);
        }
    }
    void handleHardEdges()
    {
        const auto edgeL = -SLAB_WIDTH * (0.5f - 0.001f);
        const auto edgeR =  SLAB_WIDTH * (0.5f - 0.001f);

        if ( mPos[0] < edgeL )
        {
            mPos[0] = edgeL;
            mSpeed = 0;
            //mHasCollided = true;
        }
        else
        if ( mPos[0] > edgeR )
        {
            mPos[0] = edgeR;
            mSpeed = 0;
            //mHasCollided = true;
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
    vh.mSens[Vehicle::SENS_POS_X] = vh.mPos[0];
    vh.mSens[Vehicle::SENS_SPEED] = vh.mSpeed;
    vh.mSens[Vehicle::SENS_YAW] = vh.mYawAng;
    for (size_t i=0; i < Vehicle::PROBES_N; ++i)
    {
        vh.mSens[Vehicle::SENS_PROBE_FIRST_UNITDIST + i] = 1.0f; // 1 at radius or more
        vh.mSens[Vehicle::SENS_PROBE_FIRST_VEL_X + i] = 0;
        vh.mSens[Vehicle::SENS_PROBE_FIRST_VEL_Z + i] = 0;
    }

    // arc of a probe
    const auto probeAngLen = PI2 / Vehicle::PROBES_N;

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
        // select a sensor index based on the yaw, given PROBES_N distributed
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
        const auto probeIdx = (size_t)((probeYaw / PI2) * (float)Vehicle::PROBES_N) % Vehicle::PROBES_N;

        // now that we know into which probe does the target fall, see if the distance is
        // less than the current one, and overwrite if so
        if (unitDist < vh.mSens[Vehicle::SENS_PROBE_FIRST_UNITDIST + probeIdx])
        {
            // build the velocity vector
            const auto vel = Float3{
                -other.mSpeed * sinf(other.mYawAng),
                0,
                -other.mSpeed * cosf(other.mYawAng),
            };

            vh.mSens[Vehicle::SENS_PROBE_FIRST_UNITDIST + probeIdx] = unitDist;
            vh.mSens[Vehicle::SENS_PROBE_FIRST_VEL_X + probeIdx] = vel[0];
            vh.mSens[Vehicle::SENS_PROBE_FIRST_VEL_Z + probeIdx] = vel[2];
        }
    }
}

//==================================================================
class Simulation
{
    const CS_BrainBase* const mpBrain;

    std::vector<Vehicle> mVehicles;

    double               mRunTimeS = 0;
    bool                 mHasHitVehicle = false;
    bool                 mHasHitCurb = false;
    bool                 mHasArrived = false;

public:
    Simulation(uint32_t seed, const CS_BrainBase* pBrain)
        : mpBrain(pBrain)
    {
        // 0, is our vehicle
        {
            Vehicle vh;
            vh.mPos[0] = 0;
            vh.mPos[1] = VH_ELEVATION;
            vh.mPos[2] = -4 * SLAB_DEPTH;
            vh.mIsNPC = false;
            mVehicles.push_back(vh);
        }

        // random gen and distribution
        std::mt19937 gen(seed);
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
            mVehicles.push_back(vh);
        }
    }

    void AnimateSim(float dt)
    {
        if (!IsSimRunning())
            return;

        mRunTimeS += dt;

        // animate the vehicles
        auto& ourVh = mVehicles[0];
        fillVehicleSensors(ourVh, mVehicles, 0);

        // apply the brain, if we have one 8)
        if (mpBrain)
        {
            CSM_Vec inputs(ourVh.mSens, Vehicle::SENS_N);
            CSM_Vec outputs(ourVh.mCtrls, Vehicle::CTRL_N);
            mpBrain->AnimateBrain(inputs, outputs);

            // clamp the outputs in the valid ranges
            for (auto& x : ourVh.mCtrls)
                x = glm::clamp(x, 0.f, 1.f);
        }

        for (auto& vh : mVehicles)
        {
            vh.ApplyControls(dt);
            vh.AnimateVehicle(dt);
        }

        // see if we reached the end
        if (ourVh.mPos[2] < (-SLAB_DEPTH * SLAB_END_IDX))
            mHasArrived = true;

        // check for collisions
        const auto ourMinX = ourVh.mPos[0] - VH_WIDTH * 0.5f;
        const auto ourMaxX = ourVh.mPos[0] + VH_WIDTH * 0.5f;
        const auto ourMinZ = ourVh.mPos[2] - VH_LENGTH * 0.5f;
        const auto ourMaxZ = ourVh.mPos[2] + VH_LENGTH * 0.5f;
        for (size_t i=1; i < mVehicles.size(); ++i)
        {
            const auto& vh = mVehicles[i];
            const auto minX = vh.mPos[0] - VH_WIDTH * 0.5f;
            const auto maxX = vh.mPos[0] + VH_WIDTH * 0.5f;
            const auto minZ = vh.mPos[2] - VH_LENGTH * 0.5f;
            const auto maxZ = vh.mPos[2] + VH_LENGTH * 0.5f;

            if (ourMinX < maxX && ourMaxX > minX &&
                ourMinZ < maxZ && ourMaxZ > minZ)
            {
                mHasHitVehicle = true;
                break;
            }
        }

        // hard edges
        const auto edgeL = -SLAB_WIDTH * 0.5f;
        const auto edgeR =  SLAB_WIDTH * 0.5f;
        if (ourVh.mPos[0] < edgeL || ourVh.mPos[0] > edgeR)
            mHasHitCurb = true;
    }

    double GetRunTimeS() const { return mRunTimeS; }
    bool HasHitVehicle() const { return mHasHitVehicle; }
    bool HasHitCurb() const { return mHasHitCurb; }
    bool HasArrived() const { return mHasArrived; }

    bool IsSimRunning() const
    {
        return !mHasHitVehicle && !mHasHitCurb && !mHasArrived;
    }

    double GetSimScore() const
    {
        if (mRunTimeS <= 0)
            return 0;

        const auto penaltySca = mHasHitVehicle || mHasHitCurb ? 0.1 : 1.0;
        const auto winningSca = mHasArrived ? 5.0 : 1.0;

        assert(mVehicles[0].mPos[2] <= 0);

        return -mVehicles[0].mPos[2] * penaltySca * winningSca;// / log10(mRunTimeS + 1);
    }

    const auto& GetVehicles() const { return mVehicles; }
};

#endif

