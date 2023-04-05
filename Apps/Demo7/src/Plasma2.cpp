//==================================================================
/// Plasma2.cpp
///
/// Created by Davide Pasca - 2010/11/23
/// See the file "license.txt" that comes with this project for
/// copyright info.
//==================================================================

#include <random>
#include <assert.h>
#include "DBase.h"
#include "MathBase.h"
#include "Plasma2.h"

//==================================================================
class Rand2D
{
    size_t                  mMask   {};
    size_t                  mNL2    {};
    std::vector<uint16_t>   mV1;
    std::vector<uint16_t>   mV2;

public:
    Rand2D( uint32_t seed, size_t nl2 )
    {
        c_auto n = (size_t)1 << nl2;

        mNL2 = nl2;
        mMask = n - 1;

        std::default_random_engine rndGen( seed );
        std::uniform_int_distribution<uint16_t> dist( 0 );

        mV1.resize( n );
        mV2.resize( n );

        for (size_t i=0; i < n; ++i)
            mV1[i] = dist( rndGen );

        for (size_t i=0; i < n; ++i)
            mV2[i] = dist( rndGen );
    }


    float GetVal( size_t y, size_t x ) const
    {
        c_auto v2 = (uint32_t)mV2[y & mMask];
        c_auto v1 = (uint32_t)mV1[(x+v2) & mMask];

        c_auto vv = v1 ^ v2;

        return (vv & 65535) * (1.f/65535);

        //uint32_t v1h = v1l >> (13 + ((y >> (mNL2-2)) & 3));
        //uint32_t v2h = v2l >> (13 + ((x >> (mNL2-2)) & 3));

        //return ((v1l+v1h+v2l+v2h) & 65535) * (1.f/65535);
    }
};

//==================================================================
static float _gsCosIntpl2Data[1024];

static void CosIntpl2Setup( size_t siz )
{
    assert( siz <= std::size(_gsCosIntpl2Data) );
    c_auto angStep = FM_PI / siz;
    for (size_t i=0; i < siz; ++i)
    {
        c_auto angle = i * angStep;
        _gsCosIntpl2Data[i] = (1.0f - cosf(angle)) * 0.5f;
    }
}

//==================================================================
inline float CosIntpl2(float v1, float v2, size_t idx)
{
    auto dlerp = []( c_auto &l, c_auto &r, c_auto t )
    {
        return l * (1 - t) + r * t;
    };

    return dlerp( v1, v2, _gsCosIntpl2Data[ idx ] );
}

//==================================================================
inline float CosInterpolate(float v1, float v2, float a)
{
    c_auto angle = a * (float)FM_PI;
    c_auto prc = (1.0f - cosf(angle)) * 0.5f;
    return  v1*(1.0f - prc) + v2*prc;
}

//==================================================================
static void blitStretch(
                float *pDest,
                size_t dstSizL2,
                size_t dstPitchL2,
                float scaLev,
                const float *pSrc,
                size_t srcSizL2,
                size_t srcPitchL2,
                size_t sx0,
                size_t sy0
                )
{
    c_auto srcSiz = (size_t)1 << srcSizL2;

    c_auto dsubSizL2 = (dstSizL2 - srcSizL2);
    c_auto dsubSiz = (size_t)1 << dsubSizL2;

    c_auto dsubPitchL2 = (dstPitchL2 - srcPitchL2);

    CosIntpl2Setup( dsubSiz );

    c_auto sx1 = sx0 + srcSiz;
    c_auto sy1 = sy0 + srcSiz;
    for (size_t sy=sy0; sy < sy1; ++sy)
    {
        c_auto siy0 = (sy+0) + ((sy+0) << srcPitchL2);
        c_auto siy1 = (sy+1) + ((sy+1) << srcPitchL2);

        //assert( ((sy+0) << dsubSizL2) <= 2047 );
        c_auto diy0 = (sy+0) << (dsubPitchL2 + dstPitchL2);

        for (size_t sx=sx0; sx < sx1; ++sx)
        {
            c_auto sv00 = pSrc[siy0 + (sx+0)] * scaLev;
            c_auto sv01 = pSrc[siy0 + (sx+1)] * scaLev;
            c_auto sv10 = pSrc[siy1 + (sx+0)] * scaLev;
            c_auto sv11 = pSrc[siy1 + (sx+1)] * scaLev;

            c_auto di00 = diy0 + ((sx+0)<<dsubSizL2);

            auto *pDstRowSub = pDest + di00;

            for (size_t y=0; y < dsubSiz; ++y)
            {
                c_auto sv0 = CosIntpl2( sv00, sv10, y );
                c_auto sv1 = CosIntpl2( sv01, sv11, y );
                for (size_t x=0; x < dsubSiz; ++x)
                {
                    pDstRowSub[x] = CosIntpl2( sv0, sv1, x );
                }

                pDstRowSub += (size_t)1 << dstPitchL2;
            }
        }
    }
}

//==================================================================
static void randomBlitStretchPool(
                float *pDest,
                size_t dstSizL2,
                size_t dstPitchL2,
                size_t dx0,
                size_t dy0,
                float scaLev,
                size_t srcSizL2,
                const Rand2D &randPool
                )
{
    c_auto srcSiz = (size_t)1 << srcSizL2;

    c_auto desIdxBase = (dy0 << dstPitchL2) + dx0;

    // optimize the 2 main special cases
    if ( dstSizL2 == srcSizL2 )
    {
        assert( srcSiz >= 4 );

        for (size_t sy=0; sy < srcSiz; ++sy)
        {
              auto di = desIdxBase + (sy << dstPitchL2);
            c_auto dy = dy0 + sy;
            for (size_t sx=dx0; sx < srcSiz; sx += 4, di += 4)
            {
                pDest[di+0] += randPool.GetVal(dy,sx+0) * scaLev;
                pDest[di+1] += randPool.GetVal(dy,sx+1) * scaLev;
                pDest[di+2] += randPool.GetVal(dy,sx+2) * scaLev;
                pDest[di+3] += randPool.GetVal(dy,sx+3) * scaLev;
            }
        }

        return;
    }
    else
    if ( dstSizL2 == (srcSizL2+1) )
    {
        c_auto dsubSizL2 = (size_t)1;

        for (size_t sy=0; sy < srcSiz; ++sy)
        {
            c_auto diy0 = desIdxBase + ((sy+0) << (dsubSizL2 + dstPitchL2));

            auto sv00 = randPool.GetVal(dy0+((sy+0)<<dsubSizL2), dx0) * scaLev;
            auto sv10 = randPool.GetVal(dy0+((sy+1)<<dsubSizL2), dx0) * scaLev;

            c_auto dya = dy0 + ((sy+0)<<dsubSizL2);
            c_auto dyb = dy0 + ((sy+1)<<dsubSizL2);

            for (size_t sx=0; sx < srcSiz; ++sx)
            {
                c_auto dxb = dx0 + ((sx+1)<<dsubSizL2);

                c_auto sv01 = randPool.GetVal( dya, dxb ) * scaLev;
                c_auto sv11 = randPool.GetVal( dyb, dxb ) * scaLev;

                c_auto di00 = diy0 + (sx<<dsubSizL2);

                auto *pDstRowSub = pDest + di00;

                c_auto sv0_l = sv00;
                c_auto sv0_r = sv01;
                c_auto sv1_l = (sv00+sv10) * 0.5f;
                c_auto sv1_r = (sv01+sv11) * 0.5f;

                pDstRowSub[0] += sv0_l;
                pDstRowSub[1] += (sv0_l + sv0_r) * 0.5f;

                pDstRowSub  += (size_t)1 << dstPitchL2;

                pDstRowSub[0] += sv1_l;
                pDstRowSub[1] += (sv1_l + sv1_r) * 0.5f;

                sv00 = sv01;
                sv10 = sv11;
            }
        }

        return;
    }

    c_auto dsubSizL2 = (dstSizL2 - srcSizL2);
    c_auto dsubSiz = (size_t)1 << dsubSizL2;

    CosIntpl2Setup( dsubSiz );

    for (size_t sy=0; sy < srcSiz; ++sy)
    {
        c_auto diy0 = desIdxBase + ((sy+0) << (dsubSizL2 + dstPitchL2));

        auto sv00 = randPool.GetVal(dy0+((sy+0)<<dsubSizL2), dx0) * scaLev;
        auto sv10 = randPool.GetVal(dy0+((sy+1)<<dsubSizL2), dx0) * scaLev;

        c_auto dya = dy0 + ((sy+0)<<dsubSizL2);
        c_auto dyb = dy0 + ((sy+1)<<dsubSizL2);

        for (size_t sx=0; sx < srcSiz; ++sx)
        {
            c_auto dxb = dx0 + ((sx+1)<<dsubSizL2);

            c_auto sv01 = randPool.GetVal( dya, dxb ) * scaLev;
            c_auto sv11 = randPool.GetVal( dyb, dxb ) * scaLev;

            c_auto di00 = diy0 + (sx<<dsubSizL2);

            auto *pDstRowSub = pDest + di00;

            for (size_t y=0; y < dsubSiz; ++y)
            {
                c_auto sv0 = CosIntpl2( sv00, sv10, y );
                c_auto sv1 = CosIntpl2( sv01, sv11, y );
                for (size_t x=0; x < dsubSiz; ++x)
                {
                    pDstRowSub[x] += CosIntpl2( sv0, sv1, x );
                }

                pDstRowSub  += (size_t)1 << dstPitchL2;
            }

            sv00 = sv01;
            sv10 = sv11;
        }
    }
}

//==================================================================
Plasma2::Plasma2( Params &par )
    : mPar(par)
    , mGen_oRandPool( std::make_unique<Rand2D>( par.seed, 10 ) )
    , BLOCKS_NL2(par.baseSizL2)
{
    c_auto baseGridSiz = ((size_t)1 << mPar.baseSizL2) + 1;

    // build the base grid for the main altitudes
    std::default_random_engine rndGen( par.seed );
    std::uniform_real_distribution<float> dist( 0.0, 1.f );

    mBaseGrid.resize( baseGridSiz * baseGridSiz );

    for (size_t i=0; i < mBaseGrid.size(); ++i)
        mBaseGrid[i] = dist( rndGen );

}

//
Plasma2::~Plasma2() = default;

//==================================================================
void Plasma2::RendBlock( size_t ix, size_t iy )
{
    c_auto blockDim = mPar.sizL2 - BLOCKS_NL2;

    auto scaLev = mPar.sca;

    blitStretch(
        mPar.pDest,
        blockDim,
        mPar.sizL2,
        scaLev,
        mBaseGrid.data(),
        0,
        mPar.baseSizL2,
        ix,
        iy );

    c_auto dx0 = (size_t)ix << (mPar.sizL2 - BLOCKS_NL2);
    c_auto dy0 = (size_t)iy << (mPar.sizL2 - BLOCKS_NL2);

    for (size_t d=1; d <= (mPar.sizL2-BLOCKS_NL2); ++d)
    {
        scaLev *= mPar.rough;

        randomBlitStretchPool(
            mPar.pDest,
            blockDim,
            mPar.sizL2,
            dx0,
            dy0,
            scaLev,
            d,
            *mGen_oRandPool );
    }
}

//==================================================================
bool Plasma2::IterateBlock()
{
    c_auto n = (size_t)1 << BLOCKS_NL2;

    if ( (mGen_IterIX+1) == n &&  (mGen_IterIY+1) > n )
        return false;

    RendBlock( mGen_IterIX, mGen_IterIY );

    if ( (mGen_IterIX+1) == n )
    {
        if ( (mGen_IterIY+1) == n )
            return false;
        else
        {
            mGen_IterIX = 0;
            mGen_IterIY += 1;
        }
    }
    else
        mGen_IterIX += 1;

    return true;
}

//==================================================================
bool Plasma2::IterateRow()
{
    c_auto n = (size_t)1 << BLOCKS_NL2;

    if ( (mGen_IterIY+1) > n )
        return false;

    for (size_t ix=0; ix < n; ++ix)
        RendBlock( ix, mGen_IterIY );

    if ( (mGen_IterIY+1) == n )
        return false;
    else
        mGen_IterIY += 1;

    return true;
}

