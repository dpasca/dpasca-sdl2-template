//==================================================================
/// HMapParallelOcclChecker.h
///
/// Created by Davide Pasca - 2022/06/13
/// See the file "license.txt" that comes with this project for
/// copyright info.
//==================================================================

#ifndef HMAPPARALLELOCCLCHECKER_H
#define HMAPPARALLELOCCLCHECKER_H

#include <cstdlib>
#include <cmath>
#include "DBase.h"

//==================================================================
class HMapParallelOcclChecker
{
public:
    const float *mpMap  {};
    float       mMinY   {};
    float       mMaxY   {};
    size_t      mSizL2  {};
    size_t      mMajor  {0};

    int         mLen0   {};
    int         mD0n    {};
    float       mD1n    {};
    float       mD2n    {};
    float       mOo_d1n {};

    //==================================================================
    HMapParallelOcclChecker(
        const float *pMap,
        Float3 lightDirWS,
        float minY,
        float maxY,
        size_t sizL2 ) :
            mpMap(pMap),
            mMinY(minY),
            mMaxY(maxY),
            mSizL2(sizL2)
    {
        // do we scan by x (0) or by z (2) ? Find the dominant axis
        if ( std::abs( lightDirWS[2] ) > std::abs( lightDirWS[0] ) )
        {
            mMajor = 2;
            std::swap( lightDirWS[0], lightDirWS[2] );
        }

        c_auto dim = (int)(1 << sizL2);

        c_auto coordMax = dim - 1;

        // obsolete, but we keep it for now
        constexpr int QUANT_SCALE = 1;
        const float DY_SCA = QUANT_SCALE * QUANT_SCALE;

        mLen0   = coordMax;
        mD0n    = 1;
        mD1n    = lightDirWS[1] / lightDirWS[0] * DY_SCA * (10.f / (float)dim);
        mD2n    = lightDirWS[2] / lightDirWS[0];

        if ( lightDirWS[0] < 0 )
        {
            mLen0   = -mLen0;
            mD0n    = -mD0n;
            mD1n    = -mD1n;
            mD2n    = -mD2n;
        }

        mOo_d1n = (mD1n ? 1.f/mD1n : 0);
    }

    //==================================================================
    bool IsOccludedAtPoint( int p0, int p2 ) const
    {
        c_auto sizL2 = mSizL2;
        c_auto *pMap = mpMap;

        // y value
        c_auto p1 = pMap[ (p2 << sizL2) + p0 ];

        if ( mMajor == 2 )
            std::swap( p0, p2 );

        c_auto coordMax = (int)(1 << sizL2) - 1;

        auto q0 = p0 + mLen0;

        c_auto q1 = p1 + mD1n * (float)mLen0;

        if ( q1 > mMaxY ) q0 = p0 + (int)((mMaxY - p1) * mOo_d1n); else
        if ( q1 < mMinY ) q0 = p0 + (int)((mMinY - p1) * mOo_d1n);

        if ( q0 == p0 )
            return false;

        auto i2 = (float)p2;
        auto i1 = (float)p1;

        const auto d0n = mD0n;
        const auto d1n = mD1n;
        const auto d2n = mD2n;
        if ( mMajor == 0 )
        {
            for (int i0=p0;;)
            {
                if ( i0 == q0 )
                    break;

                i0 += d0n; i1 += d1n; i2 += d2n;

                c_auto i2_w = (int)i2 & coordMax;
                c_auto i0_w = i0 & coordMax;
                if ( pMap[ (i2_w << sizL2) + i0_w ] > i1 )
                    return true;
            }
        }
        else
        {
            for (int i0=p0;;)
            {
                if ( i0 == q0 )
                    break;

                i0 += d0n; i1 += d1n; i2 += d2n;

                c_auto i2_w = (int)i2 & coordMax;
                c_auto i0_w = i0 & coordMax;
                if ( pMap[ (i0_w << sizL2) + i2_w ] > i1 )
                    return true;
            }
        }

        return false;
    }
};

#endif

