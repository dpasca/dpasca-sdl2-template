//==================================================================
/// WrapMap.h
///
/// Created by Davide Pasca - 2022/06/13
/// See the file "license.txt" that comes with this project for
/// copyright info.
//==================================================================

#ifndef WRAPMAP_H
#define WRAPMAP_H

#include "MathBase.h"

//==================================================================
template <class _T, size_t CHANS_N>
void WrapMap( _T *pMap, size_t dimL2, size_t wrapHDim )
{
    assert( wrapHDim >= 1 && wrapHDim <= ((1U << dimL2)/2) );

    auto cosLerpCoe = []( float a )
    {
        return (1.0f - cosf(a * (float)M_PI)) * 0.5f;
    };

    c_auto dim = 1 << dimL2;

    for (int i=0; i < (int)wrapHDim; ++i)
    {
        c_auto t1 = cosLerpCoe( 0.5f + 0.5f * (float)i / wrapHDim );
        c_auto t2 = cosLerpCoe( 0.5f + 0.5f * (float)(i+1) / (wrapHDim + 1) );

        c_auto i1 = i;
        c_auto i2 = dim-1 - i;

        // rows
        c_auto row1 = i1 << dimL2;
        c_auto row2 = i2 << dimL2;
        for (int j=0; j < dim; ++j)
        {
            c_auto j1 = j + row1;
            c_auto j2 = j + row2;
            for (int k=0; k < (int)CHANS_N; ++k)
            {
                c_auto jj1 = j1 * CHANS_N + k;
                c_auto jj2 = j2 * CHANS_N + k;
                c_auto val1 = pMap[ jj1 ];
                c_auto val2 = pMap[ jj2 ];
                pMap[ jj1 ] = (_T)glm::mix( val2, val1, t1 );
                pMap[ jj2 ] = (_T)glm::mix( val1, val2, t2 );
            }
        }
    }

    for (int i=0; i < (int)wrapHDim; ++i)
    {
        c_auto t1 = cosLerpCoe( 0.5f + 0.5f * (float)i / wrapHDim );
        c_auto t2 = cosLerpCoe( 0.5f + 0.5f * (float)(i+1) / (wrapHDim + 1) );

        c_auto i1 = i;
        c_auto i2 = dim-1 - i;

        // cols
        for (int j=0; j < dim; ++j)
        {
            c_auto j1 = i1 + (j << dimL2);
            c_auto j2 = i2 + (j << dimL2);
            for (int k=0; k < (int)CHANS_N; ++k)
            {
                c_auto jj1 = j1 * CHANS_N + k;
                c_auto jj2 = j2 * CHANS_N + k;
                c_auto val1 = pMap[ jj1 ];
                c_auto val2 = pMap[ jj2 ];
                pMap[ jj1 ] = (_T)glm::mix( val2, val1, t1 );
                pMap[ jj2 ] = (_T)glm::mix( val1, val2, t2 );
            }
        }
    }
}


#endif

