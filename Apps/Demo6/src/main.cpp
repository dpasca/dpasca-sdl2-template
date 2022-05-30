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
#include "MathBase.h"
#include "Voxel.h"

#include "MinimalSDLApp.h"

//#define ENABLE_DEBUG_DRAW
#define DO_SPIN_TRIANGLE

//==================================================================
#if 0
inline BBoxT calcBBoxFromVerts( const Float3 *pPos, const size_t posN )
{
    BBoxT bbox;

    bbox[0] = Float3(  FLT_MAX,  FLT_MAX,  FLT_MAX );
    bbox[1] = Float3( -FLT_MAX, -FLT_MAX, -FLT_MAX );

    // NOTE: assumes that all pos verts are relevant !
    // ..otherwise will need to check with pIndices
    for (size_t i=0; i < posN; ++i)
    {
        bbox[0] = glm::min( bbox[0], pPos[i] );
        bbox[1] = glm::max( bbox[1], pPos[i] );
    }

    return bbox;
}
#endif

//==================================================================
inline Float3 makeDeviceVert( const Matrix44 &xform,
                              const Float3 &srcPoint,
                              float deviceW,
                              float deviceH )
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
inline bool isValidDeviceVert( const Float3 &vert )
{
    return vert.z != 0.f;
}

//==================================================================
inline void drawAtom( auto *pRend, float x, float y, uint32_t col )
{
    SDL_SetRenderDrawColor( pRend, (col>>16)&0xff,(col>>8)&0xff, (col>>0)&0xff, 255 );
    constexpr int W = 7;
    constexpr int H = 7;
    SDL_Rect rc;
    rc.x = (int)(x - W/2.f);
    rc.y = (int)(y - W/2.f);
    rc.w = W;
    rc.h = H;
    SDL_RenderFillRect( pRend, &rc );
}

//==================================================================
template <typename T>
static std::array<T,8> makeCubeVerts( const T &mi, const T &ma )
{
/*
        y          2 ----- 6           __________
        |__ x    / |     / |          /|        /|
       /        3 -+--- 7  |         /_|______ / |
      z         |  0 ---+- 4        |  |______|__|
                |/      |/          | /       | /
                1 ----- 5           |/________|/
*/
    return std::array<T,8>
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
inline void voxel_DebugDraw(
                auto *pRend,
                const Voxel &vox,
                float deviceW,
                float deviceH,
                const Matrix44 &proj_obj )
{
    c_auto verts = makeCubeVerts( vox.GetVoxBBox()[0],
                                  vox.GetVoxBBox()[1] );


    auto draw3DLine = [&]( auto i, auto j )
    {
        // convert from object-space to device-space (2D display dimensions)
        c_auto v1 = makeDeviceVert( proj_obj, verts[i], deviceW, deviceH );
        c_auto v2 = makeDeviceVert( proj_obj, verts[j], deviceW, deviceH );

        if ( v1[2] > 0 && v2[2] > 0 )
            SDL_RenderDrawLine( pRend, (int)v1[0], (int)v1[1], (int)v2[0], (int)v2[1] );
    };

    SDL_SetRenderDrawColor( pRend, 0, 255, 0, 90 );

    // bottom and top
    for (int h=0; h <= 2; h += 2)
    {
        draw3DLine( 0 + h, 1 + h );
        draw3DLine( 1 + h, 5 + h );
        draw3DLine( 5 + h, 4 + h );
        draw3DLine( 4 + h, 0 + h );
    }
    // pillars
    draw3DLine( 0, 0 + 2 );
    draw3DLine( 1, 1 + 2 );
    draw3DLine( 5, 5 + 2 );
    draw3DLine( 4, 4 + 2 );
}

//==================================================================
inline void voxel_Draw(
                auto *pRend,
                const Voxel &vox,
                float deviceW,
                float deviceH,
                const Matrix44 &proj_obj )
{
    struct OutVert
    {
        Float3      pos;
        uint32_t    col;
    };
    std::vector<OutVert> xformedVerts;

    c_auto siz3 = vox.GetVoxSize();
    c_auto bbox = vox.GetVoxBBox();
    c_auto vsca = (bbox[1] - bbox[0]) / Float3( siz3[0]-1, siz3[1]-1, siz3[2]-1 );
    c_auto vtra = bbox[0];

    c_auto *pCells = vox.GetVoxCells().data();

    size_t ci = 0;
    for (size_t zi=0; zi < siz3[2]; ++zi)
    {
        c_auto z = vtra[2] + vsca[2] * (float)zi;
        for (size_t yi=0; yi < siz3[1]; ++yi)
        {
            c_auto y = vtra[1] + vsca[1] * (float)yi;
            for (size_t xi=0; xi < siz3[0]; ++xi)
            {
                c_auto val = pCells[ ci++ ];
                if NOT( val )
                    continue;

                c_auto x = vtra[0] + vsca[0] * (float)xi;

                // convert from object-space to device-space (2D display dimensions)
                c_auto deviceVert = makeDeviceVert( proj_obj, {x,y,z}, deviceW, deviceH );

                // store the vertex
                if ( deviceVert.z > 0 )
                    xformedVerts.push_back({ deviceVert, val });
            }
        }
    }

    // sort with bigger Z first
    std::sort( xformedVerts.begin(), xformedVerts.end(), []( const auto &l, const auto &r )
    {
        return l.pos[2] > r.pos[2];
    });

    // finally render the verts
    for (const auto &v : xformedVerts)
        drawAtom( pRend, v.pos[0], v.pos[1], v.col );
}

//==================================================================
static constexpr float VOXEL_HDIM       = 0.5f;      // 1 meter span
static constexpr float VOXEL_CELL_UNIT  = 0.004;

static constexpr float CAMERA_DIST      = 1.5f;     // 1.5 meters away
static constexpr float CAMERA_FOV_DEG   = 70.f;     // field of view
static constexpr float CAMERA_NEAR      = 0.01f;    // near plane (1 cm)
static constexpr float CAMERA_FAR       = 100.f;    // far plane (100 m)

//==================================================================
inline float DEG2RAD( float deg )
{
    return glm::pi<float>() / 180.f * deg;
}

//==================================================================
static void voxel_Init( auto &vox )
{
    vox.SetBBoxAndUnit( BBoxT{{ {-VOXEL_HDIM, -VOXEL_HDIM, -VOXEL_HDIM},
                                { VOXEL_HDIM,  VOXEL_HDIM,  VOXEL_HDIM}}},
                        VOXEL_CELL_UNIT,
                        10 );
};

//==================================================================
static void voxel_Update( auto &vox, size_t frameCnt )
{
    // make a vertex in voxel-space (0,0,0 -> box_min, 1,1,1 -> box_max)
    auto V = [&]( c_auto s, c_auto t, c_auto q )
    {
        c_auto voxX = glm::mix( -VOXEL_HDIM,  VOXEL_HDIM, s );
        c_auto voxY = glm::mix( -VOXEL_HDIM,  VOXEL_HDIM, t );
        c_auto voxZ = glm::mix( -VOXEL_HDIM,  VOXEL_HDIM, q );

        return Float3( voxX, voxY, voxZ );
    };

    vox.ClearVox( 0 );

    // colored cells at corners
    {
        c_auto verts = makeCubeVerts( vox.GetVoxBBox()[0],
                                      vox.GetVoxBBox()[1] );

        for (c_auto &v : verts)
            vox.SetCell( v, 0x00ff00 );
    }

    // a standing triangle
#ifdef DO_SPIN_TRIANGLE
    {
        const auto objAngX = (float)((double)frameCnt / 200.0); // in radiants
        const auto objAngY = (float)((double)frameCnt / 60.0); // in radiants

        // make the transformation for the triangle
        auto world_obj = Matrix44( 1.f );
        world_obj = glm::rotate( world_obj, objAngY, Float3( 0, 1, 0 ) );
        world_obj = glm::rotate( world_obj, objAngX, Float3( 1, 0, 0 ) );

        // make a triangle vert in world/voxel space
        auto xformV = [&]( c_auto s, c_auto t, c_auto q )
        {
            return Float3( world_obj * glm::vec4( V( s, t, q ), 1.f ) );
        };

        vox.AddTrig(
            xformV( 0.50f, 0.9f, 0.5f ),
            xformV( 0.10f, 0.1f, 0.5f ),
            xformV( 0.90f, 0.1f, 0.5f ),
            0xff0000 );
    }
#else
    vox.AddTrig(
        V( 0.50f, 0.9f, 0.5f ),
        V( 0.10f, 0.1f, 0.5f ),
        V( 0.90f, 0.1f, 0.5f ),
        0xff0000 );
#endif

    // white floor
    vox.AddQuad(
        V(0.00f, 0.f, 0.00f), V(0.00f, 0.f, 1.00f),
        V(1.00f, 0.f, 0.00f), V(1.00f, 0.f, 1.00f),
        0xe0e0e0 );

    // a flat quad bouncing up and down
    {
        c_auto y = ((float)sin( (double)frameCnt / 40 ) + 1) / 2;

        vox.AddQuad(
            V(0.10f, y, 0.10f), V(0.10f, y, 0.90f),
            V(0.90f, y, 0.10f), V(0.90f, y, 0.90f),
            0x0010ff );
    }

    // draw frame
    {
        c_auto verts = makeCubeVerts( vox.GetVoxBBox()[0],
                                      vox.GetVoxBBox()[1] );

        auto drawLine = [&]( auto i, auto j )
        {
            vox.DrawLine( verts[i], verts[j], 0x00ff00 );
        };

        // bottom and top
        for (int h=0; h <= 2; h += 2)
        {
            drawLine( 0 + h, 1 + h );
            drawLine( 1 + h, 5 + h );
            drawLine( 5 + h, 4 + h );
            drawLine( 4 + h, 0 + h );
        }
        // pillars
        drawLine( 0, 0 + 2 );
        drawLine( 1, 1 + 2 );
        drawLine( 5, 5 + 2 );
        drawLine( 4, 4 + 2 );
    }
};

//==================================================================
int main( int argc, char *argv[] )
{
    constexpr int  W = 640;
    constexpr int  H = 480;

    MinimalSDLApp app( argc, argv, W, H );

    // create the voxel
    Voxel vox;

    voxel_Init( vox );

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
        const auto objAngY = (float)((double)frameCnt / 200.0); // in radiants
        // start with identity matrix
        auto world_obj = Matrix44( 1.f );
        // concatenate static rotation around the Z angle (1,0,0)
        world_obj = glm::rotate( world_obj, DEG2RAD( 7.f ), Float3( 1, 0, 0 ) );
        // concatenate rotation around the Y angle (0,1,0)
        world_obj = glm::rotate( world_obj, objAngY, Float3( 0, 1, 0 ) );

        // --- CAMERA MATRIX ---
        // start with identity matrix
        auto camera_world = Matrix44( 1.f );
        // concatenate translation on the Z
        camera_world = glm::translate( camera_world, Float3(0.0f, 0.0f, -CAMERA_DIST) );

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

        // draw the outline
        voxel_Update( vox, frameCnt );
#ifdef ENABLE_DEBUG_DRAW
        voxel_DebugDraw( pRend, vox, W, H, proj_obj );
#endif
        // draw the voxel
        voxel_Draw( pRend, vox, W, H, proj_obj );

        // end of the frame (will present)
        app.EndFrame();

        // reasonable frame rate, since there's no vsync
        SDL_Delay( 10 );
    }

    return 0;
}

