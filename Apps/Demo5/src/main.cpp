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
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>
#include <glm/ext/matrix_transform.hpp>  // glm::translate, glm::rotate, glm::scale

#include "MinimalSDLApp.h"

//==================================================================
class Quad
{
public:
    std::array<glm::vec2,4> mVerts;

    void DrawQuad( auto *pRend )
    {
        SDL_Vertex sdlVerts[4] {};
        auto makeVtx = [&]( auto i )
        {
            sdlVerts[i].position.x = mVerts[i].x;
            sdlVerts[i].position.y = mVerts[i].y;
            sdlVerts[i].color.r = 255;
            sdlVerts[i].color.g = 0;
            sdlVerts[i].color.b = 0;
            sdlVerts[i].color.a = 255;
        };
        // make the 4 verts
        for (size_t i=0; i < 4; ++i)
            makeVtx( i );
        // make the indices for the triangulation (1 quad = 2 triangles = 6 verts)
        constexpr int idxs[6] = {0,1,2, 0,2,3};
        // draw the triangles composing the quad
        SDL_RenderGeometry( pRend, nullptr, sdlVerts, 4, idxs, 6 );
    }
};

//==================================================================
int main( int argc, char *argv[] )
{
    constexpr int  W = 640;
    constexpr int  H = 480;

    MinimalSDLApp app( argc, argv, W, H );

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

        // generate a quad
        constexpr float QUAD_SIZ = 200;
        Quad quad {
            .mVerts =
            {
                glm::vec2{-QUAD_SIZ/2, -QUAD_SIZ/2},
                glm::vec2{ QUAD_SIZ/2, -QUAD_SIZ/2},
                glm::vec2{ QUAD_SIZ/2,  QUAD_SIZ/2},
                glm::vec2{-QUAD_SIZ/2,  QUAD_SIZ/2}
            }
        };

        // build a 4x4 transformation matrix
        const auto ang = (float)((double)frameCnt / 120.0);
        auto xform = glm::mat4( 1.f );
        xform = glm::translate( xform, glm::vec3( W/2,  H/2, 0) ); // move to center screen
        xform = glm::rotate( xform, ang, glm::vec3( 0, 0, 1 ) );   // rotate on itself

        // transform the verts of the quad by the 4x4 matrix
        // NOTE: this ovewrites the original verts
        for (size_t i=0; i < 4; ++i)
        {
            auto v4 = glm::vec4( quad.mVerts[i], 0.f, 1.f ); // 2D to 4D
            v4 = xform * v4; // transform the vertex
            quad.mVerts[i] = { v4[0], v4[1] }; // store x,y of the transformed vertex
        }

        // draw the quad now that the vertices are transformed
        quad.DrawQuad( pRend );

        // end of the frame (will present)
        app.EndFrame();
    }

    return 0;
}

