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
#ifdef ENABLE_IMGUI
# define IMGUI_DEFINE_MATH_OPERATORS
# include "imgui.h"
#  include "imgui_impl_sdl.h"
# ifdef ENABLE_OPENGL
#  include "imgui_impl_opengl3.h"
#  include "imgui_impl_opengl2.h"
# else
#  include "imgui_impl_sdlrenderer.h"
# endif
# include "imgui_internal.h"
#endif
#include "MinimalSDLApp.h"

static constexpr auto TARGET_FRAME_TIME_S = 1.0 / 60;

//==================================================================
inline double getSteadyTimeSecs()
{
    return
        (double)std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now().time_since_epoch() ).count() * 1e-6;
}

#ifdef _MSC_VER
inline int strcasecmp( const char *a, const char *b ) { return _stricmp( a, b ); }
#else
# include <strings.h> // for strcasecmp()
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
  --help                       : This help
  --autoexit_delay <frames>    : Automatically exit after a number of frames
  --autoexit_savesshot <fname> : Save a screenshot on automatic exit
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
            mExitFrameN = (size_t)std::stoi( nextParam() );
        }
        else if ( isparam("--autoexit_savesshot") )
        {
            mSaveSShotPFName = nextParam();
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

#ifdef ENABLE_OPENGL
    glGetIntegerv( GL_MAJOR_VERSION, &mUsingGLVersion_Major );
    glGetIntegerv( GL_MINOR_VERSION, &mUsingGLVersion_Minor );
#endif

    // create the surface
    mpSurface = SDL_GetWindowSurface( mpWindow );
    if ( !mpSurface )
        exitErrSDL( "SDL_GetWindowSurface failed" );

    // create the renderer
    mpRenderer = SDL_CreateSoftwareRenderer( mpSurface );

    if ( !mpRenderer )
        exitErrSDL( "SDL_CreateSoftwareRenderer failed" );

#ifdef ENABLE_IMGUI
    ImGui::CreateContext();
    ImGui::StyleColorsDark();

# ifdef ENABLE_OPENGL
#  error Not yet supported
    ImGui_ImplSDL2_InitForOpenGL( mpWindow, mpGLContext );
    if ( mUsingGLVersion_Major >= 3 )
    {
        const char *pGLSLVer = mUsingGLVersion_Major > 3 || mUsingGLVersion_Minor >= 2
                                    ? "#version 150"
                                    : "#version 130";
        ImGui_ImplOpenGL3_Init( pGLSLVer );
    }
    else
    {
        ImGui_ImplOpenGL2_Init();
    }
# else
    ImGui_ImplSDL2_InitForSDLRenderer( mpWindow, mpRenderer );
    ImGui_ImplSDLRenderer_Init( mpRenderer );
# endif
#endif
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
        if ( e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_F2 )
            mShowMainUIWin = !mShowMainUIWin;

#ifdef ENABLE_IMGUI
        ImGui_ImplSDL2_ProcessEvent( &e );
#endif
        if (e.type == SDL_QUIT)
            return false;

        if ( e.type == SDL_KEYDOWN  &&  e.key.keysym.sym == SDLK_ESCAPE )
            return false;
    }

    // see if we need to exit because of the established timeout
    if ( mExitFrameN && mFrameCnt >= mExitFrameN )
    {
        printf( "Automatic exit\n" );
        if ( !mSaveSShotPFName.empty() )
        {
            printf( "Saving screenshot %s\n", mSaveSShotPFName.c_str() );
            SaveScreenshot( mSaveSShotPFName );
        }

        return false;
    }

#ifdef ENABLE_IMGUI
# ifdef ENABLE_OPENGL
    if ( mUsingGLVersion_Major >= 3 )
        ImGui_ImplOpenGL3_NewFrame();
    else
        ImGui_ImplOpenGL2_NewFrame();
# else
    ImGui_ImplSDLRenderer_NewFrame();
# endif
    ImGui_ImplSDL2_NewFrame( mpWindow );

    ImGui::NewFrame();
#endif

    return true;
}

//==================================================================
void MinimalSDLApp::EndFrame()
{
#ifdef ENABLE_IMGUI
    ImGui::Render();
# ifdef ENABLE_OPENGL
    if ( mUsingGLVersion_Major >= 3 )
        ImGui_ImplOpenGL3_RenderDrawData( ImGui::GetDrawData() );
    else
        ImGui_ImplOpenGL2_RenderDrawData( ImGui::GetDrawData() );
# else
    ImGui_ImplSDLRenderer_RenderDrawData( ImGui::GetDrawData() );
# endif
#endif

    // rudimentary frame sync, only if we're not in auto-exit mode
    if ( !mExitFrameN )
    {
        const auto curTimeS = getSteadyTimeSecs();
        const auto elapsedS = curTimeS - mLastFrameTimeS;

        if ( elapsedS < TARGET_FRAME_TIME_S )
            SDL_Delay( (uint32_t)((TARGET_FRAME_TIME_S - elapsedS) * 1000) );

        mLastFrameTimeS = curTimeS;
    }

    SDL_UpdateWindowSurface( mpWindow );

    mFrameCnt += 1;
}

//==================================================================
void MinimalSDLApp::DrawMainUIWin( const std::function<void ()> &fn )
{
#ifdef ENABLE_IMGUI
    if ( !mShowMainUIWin )
        return;

    ImGui::SetNextWindowPos( {4,4}, ImGuiCond_Once );
    if ( ImGui::Begin( "Main Win (F2: Show/hide)",
                            &mShowMainUIWin, ImGuiWindowFlags_AlwaysAutoResize ) )
    {
        fn();
    }
    ImGui::End();
#endif
}

//==================================================================
void MinimalSDLApp::SaveScreenshot( const std::string &pathFName )
{
    const auto fmt = SDL_PIXELFORMAT_RGB888;

    auto *pTmpSurf = SDL_CreateRGBSurfaceWithFormat(
                            0, mpSurface->w, mpSurface->h, 24, fmt );

    SDL_RenderReadPixels( mpRenderer, nullptr, fmt, pTmpSurf->pixels, pTmpSurf->pitch );
    SDL_SaveBMP( pTmpSurf, pathFName.c_str() );
    SDL_FreeSurface( pTmpSurf );
}
