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
#include "MathBase.h"

#include "MinimalSDLApp.h"

static bool ANIM_OBJ_POS        = true;

//==================================================================
static constexpr float HMAP_SIZL2       = 7;
static constexpr float HMAP_DISPW       = 10.f;
static constexpr float HMAP_MIN_H       = -HMAP_DISPW / 15.f;
static constexpr float HMAP_MAX_H       = -HMAP_DISPW / 10.f;

static constexpr float CAMERA_DIST      = HMAP_DISPW; // distance from center
static constexpr float CAMERA_FOV_DEG   = 70.f;     // field of view
static constexpr float CAMERA_NEAR      = 0.01f;    // near plane (1 cm)
static constexpr float CAMERA_FAR       = 1000.f;   // far plane (1000 m)

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
class HMap
{
public:
    size_t              mSizeL2 {};
    std::vector<float>  mHeights;

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
                vobj.pos = {x, dispH, y};
                vobj.siz = dxdt;

                c_auto lum = (uint8_t)glm::mix( 100.f, 255.f, h );
                vobj.col = ColType{ lum, lum, lum, 255 };

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
static void hmap_Init( auto &hmap )
{
}

//==================================================================
int main( int argc, char *argv[] )
{
    constexpr int  W = 800;
    constexpr int  H = 600;

    MinimalSDLApp app( argc, argv, W, H );

    // create the voxel
    HMap hmap( HMAP_SIZL2 );

    hmap_Init( hmap );

    // begin the main/rendering loop
    for (size_t frameCnt=0; ; ++frameCnt)
    {
        // begin the frame (or get out)
        if ( !app.BeginFrame() )
            break;

#ifdef ENABLE_IMGUI
        app.DrawMainUIWin( [&]()
        {
            ImGui::Text( "Frame: %zu", frameCnt );
            ImGui::Checkbox( "Animate obj position", &ANIM_OBJ_POS );
        } );
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

        if ( ANIM_OBJ_POS )
        {
            // move the object on the Z
            c_auto objZ = glm::mix( -0.3f, 0.5f, ((sin( frameCnt / 250.0 )+1)/2) ) * CAMERA_DIST/4;
            world_obj = glm::translate( world_obj, Float3(0.0f, 0.0f, objZ) );
        }

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

