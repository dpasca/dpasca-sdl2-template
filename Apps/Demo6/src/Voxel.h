//==================================================================
/// Voxel.h
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

#include "MathBase.h"

template <typename T> using VVec = std::vector<T>;

using VLenT = unsigned int;
using BBoxT = std::array<Float3,2>;

//==================================================================
class Voxel
{
public:
    using CellType = uint32_t;

    std::vector<CellType>  mCells;

private:
    mutable VVec<const CellType*>   mCheckRes;

public:
    BBoxT       mBBox  {};
    Float3      mUnit  {0,0,0};
private:
    Float3      mVS_LS {0,0,0}; // Voxel Space from Local Space (scale only)
    float       mOOUnitForTess = 0;
    VLenT       mN0 = 0;
    VLenT       mN1 = 0;
    VLenT       mN2 = 0;

public:
    void SetBBoxAndUnit( const BBoxT &bbox, float baseUnit, VLenT maxDimL2 );

    void AddTrigs(
            const Float3 *pPos,
            const size_t posN,
            const VVec<uint16_t> *pIndices,
            CellType val );

    void AddTrig(
            const Float3 v0,
            const Float3 v1,
            const Float3 v2,
            CellType val )
    {
        const Float3 poses[3] = { v0, v1, v2 };
        AddTrigs( poses, 3, nullptr, val );
    }

    const VVec<const CellType *> &CheckLine(
                            const Float3 &lineSta,
                            const Float3 &lineEnd ) const;

    bool FindClosestNonEmptyCellCtr(
                        const Float3 &posLS,
                        Float3 &out_foundCellCenterLS ) const;

    std::array<size_t,3> GetVoxSize() const
    {
        return { (size_t)1 << mN0, (size_t)1 << mN1, (size_t)1 << mN2 };
    }
    const auto &GetVoxBBox() const { return mBBox; }
    const auto &GetVoxCells() const { return mCells; }

private:
    void buildTessQuad(
            const Float3 &p00,
            const Float3 &p01,
            const Float3 &p10,
            const Float3 &p11,
            CellType val );

    void buildTessTri(
            const Float3 &v0,
            const Float3 &v1,
            const Float3 &v2,
            CellType val );

    void buildAddElem( const Float3 &pos, CellType val );

    bool clipLine( Float3 verts[2] ) const;
};

#endif

