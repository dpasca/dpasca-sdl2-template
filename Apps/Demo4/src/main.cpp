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

// random between -1 and 1
inline float randNorm()
{
    return randUnit() * 2 - 1;
};

//==================================================================
inline V3F makeDeviceSpacePoint( const MAT4 &xform, const V3F &srcPoint, float deviceW, float deviceH )
{
    // homogeneus coordinates (-w..w)
    const auto homo = xform * glm::vec4( srcPoint, 1.f );

    // skip if it's behind the camera
    if ( homo.z <= 0 )
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
inline bool isValidDeviceVert( const V3F &vert )
{
    return vert.z != 0.f;
}

//==================================================================
class AtomObj
{
    std::vector<V3F>    mVerts;

public:
    void AddVertex( const V3F &vert )
    {
        mVerts.push_back( vert );
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
        const auto vertsN = mVerts.size();

        // transformed verts
        std::vector<V3F> xformedVerts;
        xformedVerts.reserve( vertsN );

        // for each vertex
        for (size_t i=0; i < vertsN; ++i)
        {
            // convert from object-space to device-space (2D display dimensions)
            const auto deviceVert = makeDeviceSpacePoint( proj_obj, mVerts[i], deviceW, deviceH );

            // store the vertex
            if ( deviceVert.z > 0 )
                xformedVerts.push_back( deviceVert );
        }

        // sort with bigger Z first
        std::sort( xformedVerts.begin(), xformedVerts.end(), []( const auto &l, const auto &r )
        {
            return l.z > r.z;
        });

        // finally render the verts
        for (const auto &v : xformedVerts)
            DrawAtom( pRend, v.x, v.y, V3I(0,255,0) );
    }
};

//==================================================================
static constexpr float CUBE_SIZ         = 1.f;      // 1 meter span

static constexpr float CAMERA_DIST      = 1.5f;     // 1.5 meters away
static constexpr float CAMERA_FOV_DEG   = 70.f;     // field of view
static constexpr float CAMERA_NEAR      = 0.01f;    // near plane (1 cm)
static constexpr float CAMERA_FAR       = 100.f;    // far plane (100 m)

//==================================================================
inline float DEG2RAD( float rad )
{
    return glm::pi<float>() / 180.f * rad;
}

//==================================================================
int main( int argc, char *argv[] )
{
    constexpr int  W = 640;
    constexpr int  H = 480;

    MinimalSDLApp app( "Demo4", W, H );

    // create the object (random points)
    AtomObj obj;
    for (size_t i=0; i < 1000; ++i)
    {
        const auto x = randNorm() * CUBE_SIZ/2;
        const auto y = randNorm() * CUBE_SIZ/2;
        const auto z = randNorm() * CUBE_SIZ/2;
        obj.AddVertex( { x, y, z } );
    }

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

        // reasonable frame rate, since there's no vsync
        SDL_Delay( 10 );
    }

    return 0;
}

