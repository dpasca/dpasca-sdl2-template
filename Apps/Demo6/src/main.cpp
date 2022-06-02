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
#include "Voxels.h"
#include "VoxelsGen.h"

#include "MinimalSDLApp.h"

//#define ENABLE_DEBUG_DRAW
#define DO_SPIN_TRIANGLE
#define ANIM_OBJ_POS

//==================================================================
static constexpr float VOXEL_DIM        = 1.000f;   // 1 meter span
static constexpr float VOXEL_CELL_UNIT  = VOXEL_DIM/64;

static constexpr float CAMERA_DIST      = 1.5f;     // distance from center
static constexpr float CAMERA_FOV_DEG   = 70.f;     // field of view
static constexpr float CAMERA_NEAR      = 0.01f;    // near plane (1 cm)
static constexpr float CAMERA_FAR       = 100.f;    // far plane (100 m)

//==================================================================
// vertex coming from the object
struct VertObj
{
    Float3      pos;
    float       siz;
    uint32_t    col;
};
// vertex output in screen space
struct VertDev
{
    Float3      pos {};
    Float2      siz {};
    uint32_t    col {};
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
    SDL_SetRenderDrawColor( pRend, (c>>16)&0xff, (c>>8)&0xff, (c>>0)&0xff, 255 );
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
                const Voxels &vox,
                float deviceW,
                float deviceH,
                const Matrix44 &proj_obj )
{
    c_auto verts = makeCubeVerts( vox.GetVoxBBox()[0],
                                  vox.GetVoxBBox()[1] );


    auto draw3DLine = [&]( auto i, auto j )
    {
        // convert from object-space to device-space (2D display dimensions)
        c_auto v1 = makeDeviceVert( proj_obj, {verts[i],0.f}, deviceW, deviceH );
        c_auto v2 = makeDeviceVert( proj_obj, {verts[j],0.f}, deviceW, deviceH );

        c_auto v1p = v1.pos;
        c_auto v2p = v2.pos;

        if ( v1p[2] > 0 && v2p[2] > 0 )
            SDL_RenderDrawLineF( pRend, v1p[0], v1p[1], v2p[0], v2p[1] );
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
                const Voxels &vox,
                float deviceW,
                float deviceH,
                const Matrix44 &proj_obj )
{
    std::vector<VertDev> vertsDev;

    c_auto siz3 = vox.GetVoxSize();
    c_auto bbox = vox.GetVoxBBox();
    c_auto vsca = (bbox[1] - bbox[0]) / Float3( siz3[0]-1, siz3[1]-1, siz3[2]-1 );
    c_auto vtra = bbox[0];

    c_auto cellW = vox.GetVoxCellW();

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

                VertObj vobj;
                vobj.pos = {x,y,z};
                vobj.siz = cellW;
                vobj.col = val;

                // convert from object-space to device-space (2D display dimensions)
                c_auto vout = makeDeviceVert( proj_obj, vobj, deviceW, deviceH );

                // store the vertex
                if ( vout.pos[2] > 0 )
                    vertsDev.push_back( vout );
            }
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

//==================================================================
inline float DEG2RAD( float deg )
{
    return glm::pi<float>() / 180.f * deg;
}

//==================================================================
static void voxel_Init( auto &vox )
{
    vox.SetBBoxAndUnit( BBoxT{{ {-VOXEL_DIM/2, -VOXEL_DIM/2, -VOXEL_DIM/2},
                                { VOXEL_DIM/2,  VOXEL_DIM/2,  VOXEL_DIM/2}}},
                        VOXEL_CELL_UNIT,
                        10 );
};

//==================================================================
static void voxel_Update( auto &vox, size_t frameCnt )
{
    // make a vertex in voxel-space (0,0,0 -> box_min, 1,1,1 -> box_max)
    auto V = [&]( c_auto s, c_auto t, c_auto q )
    {
        c_auto voxX = glm::mix( -VOXEL_DIM/2,  VOXEL_DIM/2, s );
        c_auto voxY = glm::mix( -VOXEL_DIM/2,  VOXEL_DIM/2, t );
        c_auto voxZ = glm::mix( -VOXEL_DIM/2,  VOXEL_DIM/2, q );

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

        VGen_DrawTrig( vox,
            xformV( 0.50f, 0.9f, 0.5f ),
            xformV( 0.10f, 0.1f, 0.5f ),
            xformV( 0.90f, 0.1f, 0.5f ),
            0xff0000 );
    }
#else
    VGen_DrawTrig( vox,
        V( 0.50f, 0.9f, 0.5f ),
        V( 0.10f, 0.1f, 0.5f ),
        V( 0.90f, 0.1f, 0.5f ),
        0xff0000 );
#endif

    // white floor
    VGen_DrawQuad( vox,
        V(0.00f, 0.f, 0.00f), V(0.00f, 0.f, 1.00f),
        V(1.00f, 0.f, 0.00f), V(1.00f, 0.f, 1.00f),
        0xe0e0e0 );

    // a flat quad bouncing up and down
    {
        c_auto y = ((float)sin( (double)frameCnt / 40 ) + 1) / 2;

        VGen_DrawQuad( vox,
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
            VGen_DrawLine( vox, verts[i], verts[j], 0x00ff00 );
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
    constexpr int  W = 800;
    constexpr int  H = 600;

    MinimalSDLApp app( argc, argv, W, H );

    // create the voxel
    Voxels vox;

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
#ifdef ANIM_OBJ_POS
        // move the object on the Z
        c_auto objZ = glm::mix( -0.3f, 0.5f, ((sin( frameCnt / 250.0 )+1)/2) );
        world_obj = glm::translate( world_obj, Float3(0.0f, 0.0f, objZ) );
#endif
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
    }

    return 0;
}

