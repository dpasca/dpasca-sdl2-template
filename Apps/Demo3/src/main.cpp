//==================================================================
/// main.cpp
///
/// Created by Davide Pasca - 2022/05/04
/// See the file "license.txt" that comes with this project for
/// copyright info.
//==================================================================

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <array>
#include <vector>
#include <algorithm> // for std::sort

#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>
#include <glm/ext/matrix_transform.hpp>  // glm::translate, glm::rotate, glm::scale
#include <glm/ext/matrix_clip_space.hpp> // glm::perspective
#include <glm/ext/scalar_constants.hpp>  // glm::pi

#include "MinimalSDLApp.h"

using V3F = glm::vec3;
using V3I = glm::ivec3;

using MAT4 = glm::mat4;

// random between 0 and 1
inline float randUnit()
{
    return (float)(rand() % 10000) / (10000.f-1);
};

//==================================================================
static std::array<V3F,8> makeCubeVerts( const V3F &mi, const V3F &ma )
{
/*
        y          2 ----- 6           __________
        |__ x    / |     / |          /|        /|
       /        3 -+--- 7  |         /_|______ / |
      z         |  0 ---+- 4        |  |______|__|
                |/      |/          | /       | /
                1 ----- 5           |/________|/
*/
    return std::array<V3F,8>
    {
        mi,
        { mi[0], mi[1], ma[2] },
        { mi[0], ma[1], mi[2] },
        { mi[0], ma[1], ma[2] },
        { ma[0], mi[1], mi[2] },
        { ma[0], mi[1], ma[2] },
        { ma[0], ma[1], mi[2] },
        ma
    };
}

//==================================================================
inline bool isClippedHomo( const auto &homoPos )
{
    return homoPos[0] < -homoPos[3]
        || homoPos[0] >  homoPos[3]
        || homoPos[1] < -homoPos[3]
        || homoPos[1] >  homoPos[3]
        || homoPos[2] <  0
        || homoPos[2] >  homoPos[3];
}

//==================================================================
inline V3F makeDeviceSpacePoint( const MAT4 &xform, const V3F &srcPoint, float deviceW, float deviceH )
{
    // homogeneus coordinates (-w..w)
    const auto homo = xform * glm::vec4( srcPoint, 1.f );

    // clip in homogeneous space
    if ( isClippedHomo( homo ) )
        return {0,0,0};

    // convert to screen-space, meaning that anything visible is
    // in the range -1..1 for x,y and 0..1 for z
    const auto oow = 1.f / homo.w;
    const auto screenX = homo.x * oow;
    const auto screenY = homo.y * oow;
    const auto screenZ = homo.z * oow;

    // 0..deviceW (we flip vertically, from 3D standard to computer display standard)
    const auto deviceX = deviceW * (screenX + 1) * 0.5f;
    const auto deviceY = deviceH * (1 - screenY) * 0.5f;

    // generate the output vertex in device-space for X and Y and screen-space for Z
    // notice that Z is only used for sorting, so we could just use Z from homo space
    return { deviceX, deviceY, screenZ };
}

//
inline bool isValidDevicePoint3D( const V3F &vert )
{
    return vert.z != 0.f;
}

//==================================================================
class AtomObj
{
    const float         mDensity;

    std::vector<V3F>    mPoses;
    std::vector<V3I>    mCols;

public:
    AtomObj( float density ) : mDensity(density)
    {
    }

    //
    void ResetObj()
    {
        mPoses.clear();
        mCols.clear();
    }

    //
    void AddVertex( const V3F &pos, const V3I &col )
    {
        mPoses.push_back( pos );
        mCols.push_back( col );
    }

    //
    void AddLine( const V3F &p1, const V3F &p2, const V3I &col )
    {
        const auto len = glm::length( p2 - p1 );

        // minimum 1 point, max 200
        const auto n = glm::clamp( (int)(len / mDensity), 1, 200 );

        const auto oon_1 = 1.f / (float)(n-1);
        for (int i=0; i < n; ++i)
        {
            const auto t = (float)i * oon_1;

            AddVertex( glm::mix( p1, p2, t ), col );
        }
    }

    //
    void AddWireCube( float siz, const V3I &col  )
    {
        const auto verts = makeCubeVerts( V3F{ -siz/2 }, V3F{ siz/2 } );
        // bottom
        AddLine( verts[0], verts[1], col );
        AddLine( verts[1], verts[5], col );
        AddLine( verts[5], verts[4], col );
        AddLine( verts[4], verts[0], col );
        // top
        AddLine( verts[2], verts[3], col );
        AddLine( verts[3], verts[7], col );
        AddLine( verts[7], verts[6], col );
        AddLine( verts[6], verts[2], col );
        // vertical
        AddLine( verts[3], verts[1], col );
        AddLine( verts[7], verts[5], col );
        AddLine( verts[6], verts[4], col );
        AddLine( verts[2], verts[0], col );
    }

    //
    void DrawAtom( auto *pRend, float x, float y, const V3I &col )
    {
        SDL_SetRenderDrawColor( pRend, col[0], col[1], col[2], 255 );

        constexpr int W = 2;
        constexpr int H = 2;

        SDL_Rect rc;
        rc.x = (int)(x - W/2.f);
        rc.y = (int)(y - W/2.f);
        rc.w = W;
        rc.h = H;
        SDL_RenderFillRect( pRend, &rc );
    }

    //
    void DrawObj( auto *pRend, float deviceW, float deviceH, const MAT4 &proj_obj )
    {
        const auto n = mPoses.size();

        // structure used to temporarily store the transformed verts to draw
        struct OutVert
        {
            V3F pos;
            V3I col;
        };

        std::vector<OutVert> outVerts;
        outVerts.reserve( n );

        // for each vertex
        for (size_t i=0; i < n; ++i)
        {
            // convert from object-space to device-space (2D display dimensions)
            const auto devPoint = makeDeviceSpacePoint( proj_obj, mPoses[i], deviceW, deviceH );

            // make sure that it can be on display
            if ( !isValidDevicePoint3D( devPoint ) )
                continue;

            // store the position and color
            outVerts.push_back({ devPoint, mCols[i] });
        }

        // sort with bigger Z first
        std::sort( outVerts.begin(), outVerts.end(), []( const auto &l, const auto &r )
        {
            return l.pos.z > r.pos.z;
        });

        // finally render the verts
        for (const auto &v : outVerts)
            DrawAtom( pRend, v.pos.x, v.pos.y, v.col );
    }
};

//==================================================================
static constexpr float CUBE_SIZ         = 1.f;      // 1 meter span

static constexpr float CAMERA_DIST      = 2.f;      // 2 meters away
static constexpr float CAMERA_FOV_DEG   = 70.f;     // field of view
static constexpr float CAMERA_NEAR      = 0.01f;    // near plane (1 cm)
static constexpr float CAMERA_FAR       = 100.f;    // far plane (100 m)

//==================================================================
inline float DEG2RAD( float deg )
{
    return glm::pi<float>() / 180.f * deg;
}

//==================================================================
int main( int argc, char *argv[] )
{
    constexpr int  W = 640;
    constexpr int  H = 480;

    MinimalSDLApp app( argc, argv, W, H );

    AtomObj obj( CUBE_SIZ / 30.f );

    // begin the main/rendering loop
    for (size_t frameCnt=0; ; ++frameCnt)
    {
        // begin the frame (or get out)
        if ( !app.BeginFrame() )
            break;

        // get the renderer
        auto *pRend = app.GetRenderer();

        // clear the device
        SDL_SetRenderDrawColor( pRend, 0, 0, 0, 0 );
        SDL_RenderClear( pRend );

        // create the verts
        obj.ResetObj();
        obj.AddWireCube( CUBE_SIZ, {0,255,0} );

        // --- OBJECT MATRIX ---
        const auto objAngY = (float)((double)frameCnt / 120.0); // in radiants
        // start with identity matrix
        auto world_obj = MAT4( 1.f );
        // concatenate static rotation around the Z angle (1,0,0)
        world_obj = glm::rotate( world_obj, DEG2RAD( 7.f ), V3F( 1, 0, 0 ) );
        // concatenate rotation around the Y angle (0,1,0)
        world_obj = glm::rotate( world_obj, objAngY, V3F( 0, 1, 0 ) );

        // --- CAMERA MATRIX ---
        // start with identity matrix
        auto camera_world = MAT4( 1.f );
        // concatenate translation on the Z
        camera_world = glm::translate( camera_world, V3F(0.0f, 0.0f, -CAMERA_DIST) );

        // --- PROJECTION MATRIX ---
        // camera -> projection
        const auto proj_camera = glm::perspective(
                                        DEG2RAD( CAMERA_FOV_DEG ),  // FOV
                                        (float)W / H,               // aspect ratio
                                        CAMERA_NEAR,
                                        CAMERA_FAR );

        // --- FINAL MATRIX ---
        // transforming obj -> projection
        const auto proj_obj = proj_camera * camera_world * world_obj;

        // draw the object
        obj.DrawObj( pRend, W, H, proj_obj );

        // end of the frame (will present)
        app.EndFrame();
    }

    return 0;
}

