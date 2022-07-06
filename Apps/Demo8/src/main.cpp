//==================================================================
/// main.cpp
///
/// Created by Davide Pasca - 2022/07/06
/// See the file "license.txt" that comes with this project for
/// copyright info.
//==================================================================

#include <stdio.h>
#include <math.h>
#include <vector>
#include <algorithm>
#include "MinimalSDLApp.h"

static const int COLMAP_W = 32;
static const int COLMAP_H = 32;

//==================================================================
static std::vector<uint8_t> makeColMap( int mapW, int mapH )
{
    std::vector<uint8_t> m( mapH * mapW );

    for (size_t y=0; y < mapH; ++y)
    {
        for (size_t x=0; x < mapW; ++x)
        {
            const auto h =
                std::max( 0.2,
                    1 * sin( 8 * 3.1415 * (double)x / mapW ) *
                    1 * cos( 3 * 3.1415 * (double)y / mapH ) +
                    1 * cos( 5 * 3.1415 * (double)y / mapH ) );

            m[ (size_t)(y * mapW + x) ] = (uint8_t)std::clamp( 255.0 * h, 0.0, 255.0 );
        }
    }
    return m;
}

//==================================================================
static void drawDot( auto *pRend, int x, int y, uint8_t col )
{
    SDL_Rect rc;
    rc.w = 1;
    rc.h = 1;
    rc.x = x;
    rc.y = y;
    SDL_SetRenderDrawColor( pRend, col, col, col, 255 );
    SDL_RenderFillRect( pRend, &rc );
}

//==================================================================
inline auto Lerp = []( auto l, auto r, auto t )
{
    return l * (1 - t) + r * t;
};

//==================================================================
static void drawMap(
                auto *pRend,
                const auto &vec,
                int srcW,
                int srcH,
                int destXC,     // destination center X
                int destY1,     // destination top Y
                int destW_top,  // destination width at the top
                int destW_bot,  // destination width at the bottom
                int destH )     // destination height
{
    for (int destY=0; destY < destH; ++destY)
    {
        // v -> top,bottom -> 0..1
        const double v = (double)destY / destH;
        // srcY -> top,bottom -> 0..srcH
        const int srcY = (int)std::min( v * srcH, (double)srcH - 1 );

        // width of this row... interpolated from destW_top to destW_bot
        const auto destRowW = (int)Lerp( (double)destW_top, (double)destW_bot, v );

        // we want it centered, so, subtract half width from specified center
        const int destOffX = destXC - destRowW / 2;

        for (int destX=0; destX < destRowW; ++destX)
        {
            // u -> left,right -> 0..1
            const double u = (double)destX / destRowW;
            // srcX -> left,right -> 0..srcW
            const int srcX = (int)std::min( u * srcW, (double)srcW - 1 );

            // finally sample the "texture"
            const auto col = vec[ (size_t)(srcY * srcW + srcX) ];

            drawDot(
                pRend,
                destX + destOffX,
                destY + destY1,
                col );
        }
    }
}

//==================================================================
int main( int argc, char *argv[] )
{
    static int  W = 640;
    static int  H = 480;

    MinimalSDLApp app( argc, argv, W, H );

    const auto colMap = makeColMap( COLMAP_W, COLMAP_H );

    // begin the main/rendering loop
    for (size_t frameCnt=0; ; ++frameCnt)
    {
        // begin the frame (or get out)
        if ( !app.BeginFrame() )
            break;

        // get the renderer
        auto *pRend = app.GetRenderer();

        // clear the screen
        SDL_SetRenderDrawColor( pRend, 0, 0, 0, 0 );
        SDL_RenderClear( pRend );

        drawMap(
            pRend,
            colMap,   // map
            COLMAP_W, // map width
            COLMAP_H, // map height
            W / 2,    // destination draw for X (middle of screen)
            H / 2,    // destination draw for Y (somewhere below the top)
            W / 3,    // destination width at top
            W,        // destination width at bottom
            250       // destination height
        );

        // end of the frame (will present)
        app.EndFrame();
    }

    return 0;
}

