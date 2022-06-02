//==================================================================
/// Voxels.h
///
/// Created by Davide Pasca - 2022/05/29
/// See the file "license.txt" that comes with this project for
/// copyright info.
//==================================================================

#ifndef VOXELS_H
#define VOXELS_H

#include <stdint.h>
#include <array>
#include <vector>
#include <functional>
#include "MathBase.h"

//#define VOX_TEST_WORK
#if defined(VOX_TEST_WORK)
# include <assert>
# define VOXASSERT       assert
#else
# define VOXASSERT(_X_)
#endif

template <typename T> using VVec = std::vector<T>;

using VLenT = unsigned int;
using BBoxT = std::array<Float3,2>;

//==================================================================
class Voxels
{
public:
    using CellType = uint32_t;
private:
    std::vector<CellType>  mCells;
    BBoxT       mBBox  {};
    Float3      mUnit  {0,0,0};
    Float3      mVS_LS {0,0,0}; // Voxels Space from Local Space (scale only)
    float       mOOUnitForTess = 0;
    VLenT       mN0 = 0;
    VLenT       mN1 = 0;
    VLenT       mN2 = 0;

public:
    void SetBBoxAndUnit( const BBoxT &bbox, float baseUnit, VLenT maxDimL2 );

    void ClearVox( const CellType &val );

    void SetCell( const Float3 &pos, const CellType &val );

    void CheckLine(
                    const Float3 &lineSta,
                    const Float3 &lineEnd,
                    VVec<const CellType*> &out_checkRes ) const;

    bool FindClosestNonEmptyCellCtr(
                        const Float3 &posLS,
                        Float3 &out_foundCellCenterLS ) const;

    std::array<size_t,3> GetVoxSize() const
    {
        return { (size_t)1 << mN0, (size_t)1 << mN1, (size_t)1 << mN2 };
    }
    const auto &GetVoxBBox() const { return mBBox; }

    const auto &GetVS_LS() const { return mVS_LS; }

    const auto GetVoxOOUnitForTess() const { return mOOUnitForTess; }

    const auto &GetVoxCells() const { return mCells; }
          auto &GetVoxCells()       { return mCells; }

    auto GetVoxN0() const { return mN0; }
    auto GetVoxN1() const { return mN1; }
    auto GetVoxN2() const { return mN2; }

    auto GetVoxCellW() const { return mUnit[0]; }
};

//==================================================================
inline void Voxels::SetCell( const Float3 &pos, const CellType &val )
{
    VOXASSERT(
        (pos[0] >= mBBox[0][0] && pos[0] <= mBBox[1][0]) &&
        (pos[1] >= mBBox[0][1] && pos[1] <= mBBox[1][1]) &&
        (pos[2] >= mBBox[0][2] && pos[2] <= mBBox[1][2]) );

    c_auto cellIdxF = (pos - mBBox[0]) * mVS_LS;

    c_auto cell0 = (int)cellIdxF[0];
    c_auto cell1 = (int)cellIdxF[1];
    c_auto cell2 = (int)cellIdxF[2];

    if ( cell0 < 0 || cell0 >= (1 << mN0) ) return;
    if ( cell1 < 0 || cell1 >= (1 << mN1) ) return;
    if ( cell2 < 0 || cell2 >= (1 << mN2) ) return;

    mCells[ ((size_t)cell2 << (mN1 + mN0)) +
            ((size_t)cell1 <<        mN0 ) +
            ((size_t)cell0               )  ] = val;
}

//==================================================================
inline auto Voxels_ClipLineBBox = []( auto &verts, c_auto &bbox )
{
    auto halfSpace = [](auto &v, c_auto &d, c_auto lim,
                        size_t x_, size_t y0_, size_t y1_ )
    {
        c_auto x  = (glm::length_t)x_ ;
        c_auto y0 = (glm::length_t)y0_;
        c_auto y1 = (glm::length_t)y1_;

        c_auto adj = d[x] ? (lim - v[x]) / d[x] : 0.f;

        v[x] = lim;
        v[y0] += adj * d[y0];
        v[y1] += adj * d[y1];
    };

    c_auto &v0 = verts[0];
    c_auto &v1 = verts[1];

    // early check ..if both ends in the wrong quadrant
    if ((v0[0] < bbox[0][0] && v1[0] < bbox[0][0]) ||
        (v0[1] < bbox[0][1] && v1[1] < bbox[0][1]) ||
        (v0[2] < bbox[0][2] && v1[2] < bbox[0][2]) ||
        (v0[0] > bbox[1][0] && v1[0] > bbox[1][0]) ||
        (v0[1] > bbox[1][1] && v1[1] > bbox[1][1]) ||
        (v0[2] > bbox[1][2] && v1[2] > bbox[1][2]) )
    {
        return false;
    }

    // do the half-space clipping
    for (size_t i=0; i != 2; ++i)
    {
        auto &v = verts[i];
        if ( v[0] < bbox[0][0] ) halfSpace( v, verts[1]-verts[0], bbox[0][0], 0, 1, 2 );
        if ( v[1] < bbox[0][1] ) halfSpace( v, verts[1]-verts[0], bbox[0][1], 1, 2, 0 );
        if ( v[2] < bbox[0][2] ) halfSpace( v, verts[1]-verts[0], bbox[0][2], 2, 0, 1 );

        if ( v[0] > bbox[1][0] ) halfSpace( v, verts[1]-verts[0], bbox[1][0], 0, 1, 2 );
        if ( v[1] > bbox[1][1] ) halfSpace( v, verts[1]-verts[0], bbox[1][1], 1, 2, 0 );
        if ( v[2] > bbox[1][2] ) halfSpace( v, verts[1]-verts[0], bbox[1][2], 2, 0, 1 );
    }

    // last check.. returns false if it's still outside
    // (different ends outside different quadrants and line doesn't intersect bbox)
    return
         v0[0] >= bbox[0][0] && v0[1] >= bbox[0][1] && v0[2] >= bbox[0][2] &&
         v0[0] <= bbox[1][0] && v0[1] <= bbox[1][1] && v0[2] <= bbox[1][2] &&
         v1[0] >= bbox[0][0] && v1[1] >= bbox[0][1] && v1[2] >= bbox[0][2] &&
         v1[0] <= bbox[1][0] && v1[1] <= bbox[1][1] && v1[2] <= bbox[1][2];
};

//==================================================================
inline auto Voxels_LineScan = [](
            auto &vox,
            const Float3 &lineSta,
            const Float3 &lineEnd,
            const auto &onLenFn,
            const auto &onCellFn )
{
    c_auto &bbox = vox.GetVoxBBox();

    c_auto n0 = vox.GetVoxN0();
    c_auto n1 = vox.GetVoxN1();
    c_auto n2 = vox.GetVoxN2();

    // clip local-space line
    std::array<Float3,2> clipped { lineSta, lineEnd };
    if NOT( Voxels_ClipLineBBox( clipped, &bbox[0] ) )
        return; // just get out

    // voxel-space line
    Float3 lineVS[] = {
            (clipped[0] - bbox[0]) * vox.GetVS_LS(),
            (clipped[1] - bbox[0]) * vox.GetVS_LS()
    };

    c_auto maxVec = Float3( (float)((1 << n0)-1),
                            (float)((1 << n1)-1),
                            (float)((1 << n2)-1) );
    lineVS[0] = glm::clamp( lineVS[0], Float3{0,0,0}, maxVec );
    lineVS[1] = glm::clamp( lineVS[1], Float3{0,0,0}, maxVec );

    auto diff = lineVS[1] - lineVS[0];

    VLenT ia, ta;
    VLenT ib, tb;
    VLenT ic, tc;
#if defined(VOX_TEST_WORK)
    VLenT nna, nnb, nnc;
#endif

    c_auto adiff0 = fabs( diff[0] );
    c_auto adiff1 = fabs( diff[1] );
    c_auto adiff2 = fabs( diff[2] );

    if ( adiff0 > adiff1 )
    {
        if ( adiff0 > adiff2 ) {
            ia = 0; ta = 0;
            ib = 1; tb = n0;
            ic = 2; tc = n0 + n1;
#if defined(VOX_TEST_WORK)
            nna = 1 << n0;
            nnb = 1 << n1;
            nnc = 1 << n2;
#endif
        } else {
            ia = 2; ta = n0 + n1;
            ib = 0; tb = 0;
            ic = 1; tc = n0;
#if defined(VOX_TEST_WORK)
            nna = 1 << n2;
            nnb = 1 << n0;
            nnc = 1 << n1;
#endif
        }
    }
    else
    {
        if ( adiff1 > adiff2 ) {
            ia = 1; ta = n0;
            ib = 2; tb = n0 + n1;
            ic = 0; tc = 0;
#if defined(VOX_TEST_WORK)
            nna = 1 << n1;
            nnb = 1 << n2;
            nnc = 1 << n0;
#endif
        } else {
            ia = 2; ta = n0 + n1;
            ib = 0; tb = 0;
            ic = 1; tc = n0;
#if defined(VOX_TEST_WORK)
            nna = 1 << n2;
            nnb = 1 << n0;
            nnc = 1 << n1;
#endif
        }
    }

    if ( lineVS[0][ia] > lineVS[1][ia] )
    {
        std::swap( lineVS[0], lineVS[1] );
        diff = -diff;
    }

    c_auto a0 = (VLenT)lineVS[0][ia];
    c_auto a1 = (VLenT)lineVS[1][ia];

    //float fa = lineVS[0][ia] - (float)a0;

    c_auto lena = a1 - a0 + 1;

    onLenFn( lena );

    c_auto oolena = 1.f / (float)lena;

    c_auto db = diff[ib] * oolena;
    c_auto dc = diff[ic] * oolena;

    auto b = lineVS[0][ib];// + fa * db;
    auto c = lineVS[0][ic];// + fa * dc;


    auto &cells = vox.GetVoxCells();

    for (VLenT a=a0; a <= a1; a += 1, b += db, c += dc)
    {
#if defined(VOX_TEST_WORK)
        VOXASSERT( (VLenT)a < nna && (VLenT)b < nnb && (VLenT)c < nnc );
#endif
        c_auto a_idx = (size_t)a << ta;
        c_auto b_idx = (size_t)b << tb;
        c_auto c_idx = (size_t)c << tc;

        onCellFn( a-a0, cells[ a_idx + b_idx + c_idx ] );
    }
};

#endif

