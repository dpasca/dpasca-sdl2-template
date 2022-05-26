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
    MinimalSDLApp( int argc, char *argv[], int w, int h );
    ~MinimalSDLApp();

    bool BeginFrame();
    void EndFrame();

    auto *GetRenderer() { return mpRenderer; }
};


#endif

