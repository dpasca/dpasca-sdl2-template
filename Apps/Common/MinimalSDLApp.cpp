//==================================================================
/// MinimalSDLApp.cpp
///
/// Created by Davide Pasca - 2022/05/27
/// See the file "license.txt" that comes with this project for
/// copyright info.
//==================================================================

#include <string>
#include <filesystem>
#include "MinimalSDLApp.h"

namespace fs = std::filesystem;

// see: https://stackoverflow.com/a/56833374/1507238
inline auto StrFromU8Str(const std::string &s) { return s; }
inline auto StrFromU8Str(std::string &&s) { return std::move(s); }
#if defined(__cpp_lib_char8_t)
inline auto StrFromU8Str(const std::u8string &s)
{
	return std::string( s.begin(), s.end() );
}
#endif

//==================================================================
MinimalSDLApp::MinimalSDLApp( int argc, char *argv[], int w, int h )
{
    const auto title = StrFromU8Str( fs::path(argv[0]).filename().stem().u8string() );

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
                        title.c_str(),
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
MinimalSDLApp::~MinimalSDLApp()
{
    SDL_Quit();
}

//==================================================================
bool MinimalSDLApp::BeginFrame()
{
    SDL_Event e;
    while ( SDL_PollEvent(&e) )
    {
        if (e.type == SDL_QUIT)
            return false;

        if ( e.type == SDL_KEYDOWN  &&  e.key.keysym.sym == SDLK_ESCAPE )
            return false;
    }
    return true;
}

//==================================================================
void MinimalSDLApp::EndFrame()
{
    SDL_UpdateWindowSurface( mpWindow );
}

