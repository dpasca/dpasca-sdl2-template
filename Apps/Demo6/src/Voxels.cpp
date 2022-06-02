//==================================================================
/// Voxels.h
///
/// Created by Davide Pasca - 2022/05/29
/// See the file "license.txt" that comes with this project for
/// copyright info.
//==================================================================

#include <float.h>
#include "Voxels.h"

//==================================================================
inline VLenT log2ceil( VLenT val )
{
    for (VLenT i=0; i < 32; ++i)
    {
        if ( ((VLenT)1<<i) >= val )
            return i;
    }

    VOXASSERT( 0 );
    return (VLenT)-1;
}

//==================================================================
void Voxels::SetBBoxAndUnit( const BBoxT &bbox, float baseUnit, VLenT maxDimL2 )
{
    mBBox = bbox;

    c_auto bboxSiz = mBBox[1] - mBBox[0];

    mN0 = log2ceil( (VLenT)ceilf(bboxSiz[0] / baseUnit) );
    mN1 = log2ceil( (VLenT)ceilf(bboxSiz[1] / baseUnit) );
    mN2 = log2ceil( (VLenT)ceilf(bboxSiz[2] / baseUnit) );

    if ( maxDimL2 != 0 )
    {
        mN0 = std::min( mN0, maxDimL2 );
        mN1 = std::min( mN1, maxDimL2 );
        mN2 = std::min( mN2, maxDimL2 );
    }

    c_auto nn0 = (float)(1 << mN0);
    c_auto nn1 = (float)(1 << mN1);
    c_auto nn2 = (float)(1 << mN2);

    mCells.clear();
    mCells.resize( (size_t)1 << (mN0 + mN1 + mN2) );

    mUnit[0] = nn0 > 1 ? (bboxSiz[0] / (nn0-1)) : 0.f;
    mUnit[1] = nn1 > 1 ? (bboxSiz[1] / (nn1-1)) : 0.f;
    mUnit[2] = nn2 > 1 ? (bboxSiz[2] / (nn2-1)) : 0.f;

    mVS_LS[0] = (nn0 - 1) / (bboxSiz[0] ? bboxSiz[0] : 1);
    mVS_LS[1] = (nn1 - 1) / (bboxSiz[1] ? bboxSiz[1] : 1);
    mVS_LS[2] = (nn2 - 1) / (bboxSiz[2] ? bboxSiz[2] : 1);

    // avoid picking a 0 potential 2D gementry aligned to some axis
    auto minUnit = FLT_MAX;
    c_auto EPS = 0.0001f;
    if ( mUnit[0] > EPS ) minUnit = std::min( minUnit, mUnit[0] );
    if ( mUnit[1] > EPS ) minUnit = std::min( minUnit, mUnit[1] );
    if ( mUnit[2] > EPS ) minUnit = std::min( minUnit, mUnit[2] );

    VOXASSERT( minUnit > EPS && minUnit != FLT_MAX );

    mOOUnitForTess = minUnit ? (1.f / minUnit * 1.00f) : 1.f;
}

//==================================================================
void Voxels::ClearVox( const CellType &val )
{
    std::fill( mCells.begin(), mCells.end(), val );
}

//==================================================================
bool Voxels::FindClosestNonEmptyCellCtr(
                        const Float3 &posLS,
                        Float3 &out_foundCellCenterLS ) const
{
    c_auto nn0 = 1 << mN0;
    c_auto nn1 = 1 << mN1;
    c_auto nn2 = 1 << mN2;

    // position to check in Voxels Space
    auto posVS = mVS_LS * (posLS - mBBox[0]);

    auto  closestSqr   = FLT_MAX;
    auto  closestCtrVS = Float3( 0, 0, 0 );

    for (int i2=0; i2 < nn2; ++i2)
    {
        c_auto *pCell_I2 = &mCells[ i2 << (mN1 + mN0) ];

        for (int i1=0; i1 < nn1; ++i1)
        {
            c_auto *pCell_I21 = &pCell_I2[ i1 << mN0 ];
            for (int i0=0; i0 < nn0; ++i0)
            {
                c_auto &cell = pCell_I21[ i0 ];
                // make sure that it's not empty
                if NOT( cell )
                    continue;

                // center of the cell in Voxels Space
                const Float3 cellCtrVS(
                            ((float)i0+0.5f),
                            ((float)i1+0.5f),
                            ((float)i2+0.5f) );

                c_auto distSqr = lengthSqr( cellCtrVS - posVS );
                if ( distSqr < closestSqr )
                {
                    closestSqr = distSqr;
                    closestCtrVS = cellCtrVS;
                }
            }
        }
    }

    if ( closestSqr == FLT_MAX )
        return false;

    // give out the center of the cell in Local Space
    out_foundCellCenterLS = closestCtrVS * mUnit + mBBox[0];

    return true;
}

//==================================================================
void Voxels::CheckLine(
                    const Float3 &lineSta,
                    const Float3 &lineEnd,
                    VVec<const CellType*> &out_checkRes ) const
{
    out_checkRes.clear();

    Voxels_LineScan(
        *this,
        lineSta,
        lineEnd,
        [&]( c_auto len )             { out_checkRes.resize( len ); },
        [&]( c_auto idx, c_auto &desVal ){ out_checkRes[ idx ] = &desVal; } );
}

