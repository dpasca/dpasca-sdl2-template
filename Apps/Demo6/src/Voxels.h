//==================================================================
/// Voxels.h
///
/// Created by Davide Pasca - 2022/05/29
/// See the file "license.txt" that comes with this project for
/// copyright info.
//==================================================================

#ifndef VOXEL_H
#define VOXEL_H

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

    std::vector<CellType>  mCells;

public:
    BBoxT       mBBox  {};
    Float3      mUnit  {0,0,0};
private:
    Float3      mVS_LS {0,0,0}; // Voxels Space from Local Space (scale only)
    float       mOOUnitForTess = 0;
    VLenT       mN0 = 0;
    VLenT       mN1 = 0;
    VLenT       mN2 = 0;

public:
    void SetBBoxAndUnit( const BBoxT &bbox, float baseUnit, VLenT maxDimL2 );

    void ClearVox( const CellType &val );

    void SetCell( const Float3 &pos, const CellType &val );

    void AddTrigs(
            const Float3 *pPos,
            const size_t posN,
            const VVec<uint16_t> *pIndices,
            const CellType &val );

    void AddTrig(
            const Float3 v0,
            const Float3 v1,
            const Float3 v2,
            const CellType &val )
    {
        const Float3 poses[3] = { v0, v1, v2 };
        AddTrigs( poses, 3, nullptr, val );
    }

    void AddQuad(
            const Float3 &p00,
            const Float3 &p01,
            const Float3 &p10,
            const Float3 &p11,
            const CellType &val );

    void CheckLine(
                    const Float3 &lineSta,
                    const Float3 &lineEnd,
                    VVec<const CellType*> &out_checkRes ) const;

    void DrawLine(
                    const Float3 &lineSta,
                    const Float3 &lineEnd,
                    const CellType &srcVal );

    bool FindClosestNonEmptyCellCtr(
                        const Float3 &posLS,
                        Float3 &out_foundCellCenterLS ) const;

    std::array<size_t,3> GetVoxSize() const
    {
        return { (size_t)1 << mN0, (size_t)1 << mN1, (size_t)1 << mN2 };
    }
    const auto &GetVoxBBox() const { return mBBox; }

    const auto &GetVS_LS() const { return mVS_LS; }

    const auto &GetVoxCells() const { return mCells; }
          auto &GetVoxCells()       { return mCells; }

    auto GetVoxN0() const { return mN0; }
    auto GetVoxN1() const { return mN1; }
    auto GetVoxN2() const { return mN2; }

    auto GetVoxCellW() const { return mUnit[0]; }

private:
    void buildTessTri(
            const Float3 &v0,
            const Float3 &v1,
            const Float3 &v2,
            const CellType &val );
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

#endif
