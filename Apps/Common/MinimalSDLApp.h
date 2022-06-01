//==================================================================
/// MinimalSDLApp.h
///
/// Created by Davide Pasca - 2022/05/04
/// See the file "license.txt" that comes with this project for
/// copyright info.
//==================================================================

#ifndef MINIMALSDLAPP_H
#define MINIMALSDLAPP_H

#include <string>
#include "SDL.h"

//==================================================================
class MinimalSDLApp
{
    SDL_Window      *mpWindow   {};
    SDL_Surface     *mpSurface  {};
    SDL_Renderer    *mpRenderer {};

    size_t          mFrameCnt       {};
    double          mLastFrameTimeS {};

    size_t          mExitFrameN {};
    std::string     mSaveSShotPFName;

public:
    MinimalSDLApp( int argc, char *argv[], int w, int h );
    ~MinimalSDLApp();

    bool BeginFrame();
    void EndFrame();

    auto *GetRenderer() { return mpRenderer; }

    void SaveScreenshot( const std::string &pathFName );

private:
    void ctor_parseArgs( int argc, char *argv[] );
};


#endif

