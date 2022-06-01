//==================================================================
/// main.cpp
///
/// Created by Davide Pasca - 2022/05/04
/// See the file "license.txt" that comes with this project for
/// copyright info.
//==================================================================

#include <stdio.h>
#include <math.h>
#include "MinimalSDLApp.h"

//==================================================================
int main( int argc, char *argv[] )
{
    static int  W = 640;
    static int  H = 480;

    MinimalSDLApp app( argc, argv, W, H );

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

        // float up and down
        auto animY = H/10.0 * sin( (double)frameCnt / 50.0 );

        // paint the rect
        SDL_Rect rc;
        rc.w = W / 8;
        rc.h = H / 8;
        rc.x =                        (W - rc.w) / 2;
        rc.y = (int)(animY + (double)((H - rc.h) / 2));
        SDL_SetRenderDrawColor( pRend, 255, 0, 0, 255 );
        SDL_RenderFillRect( pRend, &rc );

        // end of the frame (will present)
        app.EndFrame();
    }

    return 0;
}

