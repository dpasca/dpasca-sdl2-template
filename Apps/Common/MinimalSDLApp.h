//==================================================================
/// MinimalSDLApp.h
///
/// Created by Davide Pasca - 2022/05/04
/// See the file "license.txt" that comes with this project for
/// copyright info.
//==================================================================

#ifndef MINIMALSDLAPP_H
#define MINIMALSDLAPP_H

#include "SDL.h"

//==================================================================
class MinimalSDLApp
{
    SDL_Window      *mpWindow   {};
    SDL_Surface     *mpSurface  {};
    SDL_Renderer    *mpRenderer {};

public:
    //==================================================================
    MinimalSDLApp( const char *pTitle, int w, int h )
    {
        // enable logging
        SDL_LogSetPriority( SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_INFO );

        //
        auto exitErrSDL = [&]( const char *pMsg )
        {
            SDL_LogError( SDL_LOG_CATEGORY_APPLICATION, "%s: %s\n", pMsg, SDL_GetError() );
            exit( 1 );
        };

        // initialize SDL
        if ( SDL_Init(SDL_INIT_VIDEO) )
            exitErrSDL( "SDL_Init failed" );

        // create the window
        mpWindow = SDL_CreateWindow(
                            pTitle,
                            SDL_WINDOWPOS_UNDEFINED,
                            SDL_WINDOWPOS_UNDEFINED,
                            w, h, 0);
        if ( !mpWindow )
            exitErrSDL( "Window creation fail" );

        // create the surface
        mpSurface = SDL_GetWindowSurface( mpWindow );
        if ( !mpSurface )
            exitErrSDL( "SDL_GetWindowSurface failed" );

        // create the renderer
        mpRenderer = SDL_CreateSoftwareRenderer( mpSurface );

        if ( !mpRenderer )
            exitErrSDL( "SDL_CreateSoftwareRenderer failed" );
    }

    //==================================================================
    ~MinimalSDLApp()
    {
        SDL_Quit();
    }

    //==================================================================
    bool BeginFrame()
    {
        SDL_Event e;
        while (SDL_PollEvent(&e))
        {
            if (e.type == SDL_QUIT)
                return false;

            if ( e.type == SDL_KEYDOWN  &&  e.key.keysym.sym == SDLK_ESCAPE )
                return false;
        }
        return true;
    }

    void EndFrame()
    {
        SDL_UpdateWindowSurface( mpWindow );
    }

    auto *GetRenderer()
    {
        return mpRenderer;
    }
};


#endif

