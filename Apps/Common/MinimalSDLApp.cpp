//==================================================================
/// MinimalSDLApp.cpp
///
/// Created by Davide Pasca - 2022/05/27
/// See the file "license.txt" that comes with this project for
/// copyright info.
//==================================================================

#include <string>
#include <chrono>
#include <filesystem>
#include "MinimalSDLApp.h"

//==================================================================
inline double getSteadyTimeSecs()
{
    return
        (double)std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now().time_since_epoch() ).count() * 1e-6;
}

#ifdef _MSC_VER
inline int strcasecmp( const char *a, const char *b ) { return _stricmp( a, b ); }
#endif

//==================================================================
void MinimalSDLApp::ctor_parseArgs( int argc, char *argv[] )
{
    auto printUsage = [&]( const char *pMsg )
    {
        printf( R"RAW(
Usage
  %s [options]

Options
  --help                      : This help
  --autoexit_delay <secs>     : Automatically exit after a number of seconds
)RAW", argv[0] );

        if ( pMsg )
            printf( "\n%s\n", pMsg );
    };

	for (int i=1; i < argc; ++i)
	{
        auto isparam = [&]( const auto &src ) { return !strcasecmp( argv[i], src ); };

        auto nextParam = [&]()
        {
			if ( ++i >= argc )
            {
				printUsage( "Missing parameters ?" );
                exit( 1 );
            }
            return argv[i];
        };

        if ( isparam("--help") )
        {
            printUsage(0);
            exit( 0 );
        }
        else if ( isparam("--autoexit_delay") )
        {
            mExitSteadyTimeS = getSteadyTimeSecs() + std::stod( nextParam() );
        }
    }
}

//==================================================================
static std::string getFNameStem( const char *pPathFName )
{
    const auto str = std::filesystem::path( pPathFName ).filename().stem().u8string();
#if defined(__cpp_lib_char8_t)
	return std::string( str.begin(), str.end() );
#else
    return str;
#endif
}

//==================================================================
MinimalSDLApp::MinimalSDLApp( int argc, char *argv[], int w, int h )
{
    ctor_parseArgs( argc, argv );

    // get the title from arg 0
    const auto title = getFNameStem( argv[0] );

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

    // see if we need to exit because of the established timeout
    if ( mExitSteadyTimeS && getSteadyTimeSecs() > mExitSteadyTimeS )
    {
        printf( "Automatic exit\n" );
        return false;
    }

    return true;
}

//==================================================================
void MinimalSDLApp::EndFrame()
{
    SDL_UpdateWindowSurface( mpWindow );
}

