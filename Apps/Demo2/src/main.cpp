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
#include <vector>
#include "MinimalSDLApp.h"

// random between 0 and 1
inline float randUnit()
{
    return (float)(rand() % 10000) / (10000.f-1);
};

// random between -1 and 1
inline float randNorm()
{
    return randUnit() * 2 - 1;
};

// just a lerp
inline auto dlerp = []( const auto &l, const auto &r, const auto t )
{
    return l * (1 - t) + r * t;
};

static float STAR_VEL_MIN = 0.5f;
static float STAR_VEL_MAX = 2.5f;

//==================================================================
struct Star
{
    float mX    {};
    float mY    {};
    float mZ    {};
    float mVelZ {};

    void InitStar( float z )
    {
        mX = randNorm();
        mY = randNorm();
        mZ = z;
        mVelZ = -dlerp( STAR_VEL_MIN, STAR_VEL_MAX, randUnit() );
    }

    void AnimStar()
    {
        mZ += mVelZ;
    }

    void DrawStar( auto *pRend, float x, float y, float lum )
    {
        const auto lumI = (int)std::min( lum * 255, 255.f );
        SDL_SetRenderDrawColor( pRend, lumI, lumI, lumI, 255 );

        constexpr int W = 2;
        constexpr int H = 2;

        SDL_Rect rc;
        rc.x = (int)(x - W/2.f);
        rc.y = (int)(y - W/2.f);
        rc.w = W;
        rc.h = H;
        SDL_RenderFillRect( pRend, &rc );
    }
};

//==================================================================
static constexpr float FIELD_FAR  = 1000;
static constexpr float FIELD_NEAR = 10;
static constexpr float FIELD_WIDTH = 500;

// initialize the vector of stars
static void starsInit( auto &stars )
{
    for (auto &s : stars)
        s.InitStar( randUnit() * FIELD_FAR );
}

// animate, by moving and regenerating when necessary
static void starsAnim( auto &stars )
{
    for (auto &s : stars)
    {
        s.AnimStar();

        // re initialize the star if it went too close
        if ( s.mZ < FIELD_NEAR )
            s.InitStar( FIELD_FAR );
    }
}

// transform and draw the starfield
static void starsDraw( auto &stars, auto *pRend, float screenW, float screenH )
{
    for (auto &s : stars)
    {
        if ( s.mZ < FIELD_NEAR )
            continue;

        // perspective projection of the star point
        const auto projX = FIELD_WIDTH * s.mX / s.mZ;
        const auto projY = FIELD_WIDTH * s.mY / s.mZ;

        // convert -1..1 range to 0..screenW/H-1
        const auto screenX = (screenW-1) * (projX + 1) * 0.5f;
        const auto screenY = (screenH-1) * (projY + 1) * 0.5f;

        // luminosity from depth
        const auto depthUnit = (s.mZ - FIELD_NEAR) / (FIELD_FAR - FIELD_NEAR);
        const auto lum = dlerp( 0.1f, 1.0f, 1 - depthUnit );

        // draw the actual point
        s.DrawStar( pRend, screenX, screenY, lum );
    }
}

//==================================================================
int main( int argc, char *argv[] )
{
    constexpr int  W = 800;
    constexpr int  H = 600;

    MinimalSDLApp app( argc, argv, W, H );

    std::vector<Star> stars( 2000 );

    // initialize the stars
    starsInit( stars );

    // begin the main/rendering loop
    for (size_t frameCnt=0; ; ++frameCnt)
    {
        // begin the frame (or get out)
        if ( !app.BeginFrame() )
            break;

#ifdef ENABLE_IMGUI
        app.DrawMainUIWin( [&]()
        {
            static float dude2[] {0,0};
            ImGui::InputFloat( "Vel Min", &STAR_VEL_MIN, 0.1f, 1.0f );
            ImGui::InputFloat( "Vel Max", &STAR_VEL_MAX, 0.1f, 1.0f );
            STAR_VEL_MIN = std::max( 0.01f, std::min( STAR_VEL_MIN, 10.f ) );
            STAR_VEL_MAX = std::max( 0.01f, std::min( STAR_VEL_MAX, 10.f ) );
            if ( ImGui::Button( "Reset" ) )
                starsInit( stars );
        } );
#endif
        // get the renderer
        auto *pRend = app.GetRenderer();

        // clear the screen
        SDL_SetRenderDrawColor( pRend, 0, 0, 0, 0 );
        SDL_RenderClear( pRend );

        // animate the stars
        starsAnim( stars );

        // draw the stars
        starsDraw( stars, pRend, (float)W, (float)H );

        // end of the frame (will present)
        app.EndFrame();
    }

    return 0;
}

