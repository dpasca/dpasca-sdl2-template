//==================================================================
/// Voxel.h
///
/// Created by Davide Pasca - 2022/05/29
/// See the file "license.txt" that comes with this project for
/// copyright info.
//==================================================================

#include "Voxel.h"

//#define TEST_WORK

#if defined(TEST_WORK)
# include <assert>
# define VOXASSERT       assert
#else
# define VOXASSERT(_X_)
#endif

//==================================================================
inline auto lengthSqr = []( c_auto &v ) { return glm::dot( v, v ); };

//==================================================================
inline void halfSpace3(
                    Float3 &v,
                    const Float3 &d,
                    float lim,
                    size_t x_,
                    size_t y0_,
                    size_t y1_ )
{
    c_auto x  = (glm::length_t)x_ ;
    c_auto y0 = (glm::length_t)y0_;
    c_auto y1 = (glm::length_t)y1_;

    c_auto adj = d[x] ? (lim - v[x]) / d[x] : 0.f;

    v[x] = lim;
    v[y0] += adj * d[y0];
    v[y1] += adj * d[y1];
}

//==================================================================
inline bool clipLineBBox( auto &verts, c_auto &bbox )
{
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
        if ( v[0] < bbox[0][0] ) halfSpace3( v, verts[1]-verts[0], bbox[0][0], 0, 1, 2 );
        if ( v[1] < bbox[0][1] ) halfSpace3( v, verts[1]-verts[0], bbox[0][1], 1, 2, 0 );
        if ( v[2] < bbox[0][2] ) halfSpace3( v, verts[1]-verts[0], bbox[0][2], 2, 0, 1 );

        if ( v[0] > bbox[1][0] ) halfSpace3( v, verts[1]-verts[0], bbox[1][0], 0, 1, 2 );
        if ( v[1] > bbox[1][1] ) halfSpace3( v, verts[1]-verts[0], bbox[1][1], 1, 2, 0 );
        if ( v[2] > bbox[1][2] ) halfSpace3( v, verts[1]-verts[0], bbox[1][2], 2, 0, 1 );
    }

    // last check.. returns false if it's still outside
    // (different ends outside different quadrants and line doesn't intersect bbox)
    return
         v0[0] >= bbox[0][0] && v0[1] >= bbox[0][1] && v0[2] >= bbox[0][2] &&
         v0[0] <= bbox[1][0] && v0[1] <= bbox[1][1] && v0[2] <= bbox[1][2] &&
         v1[0] >= bbox[0][0] && v1[1] >= bbox[0][1] && v1[2] >= bbox[0][2] &&
         v1[0] <= bbox[1][0] && v1[1] <= bbox[1][1] && v1[2] <= bbox[1][2];
}


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
void Voxel::SetBBoxAndUnit( const BBoxT &bbox, float baseUnit, VLenT maxDimL2 )
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

    mUnit[0] = bboxSiz[0] / nn0;
    mUnit[1] = bboxSiz[1] / nn1;
    mUnit[2] = bboxSiz[2] / nn2;

    mVS_LS[0] = nn0 / (bboxSiz[0] ? bboxSiz[0] : 1);
    mVS_LS[1] = nn1 / (bboxSiz[1] ? bboxSiz[1] : 1);
    mVS_LS[2] = nn2 / (bboxSiz[2] ? bboxSiz[2] : 1);

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
void Voxel::ClearVox( CellType val )
{
    std::fill( mCells.begin(), mCells.end(), val );
}

//==================================================================
void Voxel::AddTrigs(
            const Float3 *pPos,
            const size_t posN,
            const VVec<uint16_t> *pIndices,
            CellType val )
{
    VOXASSERT( (pIndices && (*pIndices).size() % 3 == 0) || posN % 3 == 0 );

    if ( pIndices )
    {
        c_auto &idxs = *pIndices;
        for (size_t i=0, j=0; i != idxs.size(); i += 3, j += 1)
            buildTessTri( pPos[idxs[i+0]], pPos[idxs[i+1]], pPos[idxs[i+2]], val );
    }
    else
    {
        for (size_t i=0, j=0; i != posN; i += 3, j += 1)
            buildTessTri( pPos[i+0], pPos[i+1], pPos[i+2], val );
    }
}

//==================================================================
inline void Voxel::buildAddElem( const Float3 &pos, CellType val )
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
void Voxel::buildTessQuad(
        const Float3 &p00,
        const Float3 &p01,
        const Float3 &p10,
        const Float3 &p11,
        CellType val )
{
    c_auto dh0 = p01 - p00;
    c_auto dh1 = p11 - p10;
    c_auto dv0 = p10 - p00;
    c_auto dv1 = p11 - p01;

    c_auto msqr_h0 = lengthSqr( dh0 );
    c_auto msqr_h1 = lengthSqr( dh1 );
    c_auto msqr_v0 = lengthSqr( dv0 );
    c_auto msqr_v1 = lengthSqr( dv1 );

    c_auto maxh = sqrtf( std::max( msqr_h0, msqr_h1 ) );
    c_auto maxv = sqrtf( std::max( msqr_v0, msqr_v1 ) );

    // degenerate case.. when it's a line or a point
    static constexpr auto EPS = 0.0001f;
    if ( maxh < EPS || maxv < EPS )
        return;

    c_auto nhf = ceilf( maxh * mOOUnitForTess );
    c_auto nvf = ceilf( maxv * mOOUnitForTess );
    //VOXASSERT( nhf != 0 && nvf != 0 );

    // NOTE: this 0.5 is a bit hacky, it works for how it's used
    //  to split a triangle
    if ( nhf < 0.5f || nvf < 0.5f )
        return;

    c_auto oonhf = 1.f / (nhf - 0.5f);
    c_auto oonvf = 1.f / (nvf - 0.5f);

    c_auto ddv0 = dv0 * oonvf;
    c_auto ddv1 = dv1 * oonvf;
      auto pv0 = p00;
      auto pv1 = p01;

    c_auto nh = (VLenT)nhf;
    c_auto nv = (VLenT)nvf;
    for (VLenT i=0; i < nv; ++i, pv0 += ddv0,
                                 pv1 += ddv1)
    {
        c_auto ddh = (pv1 - pv0) * oonhf;
          auto ph = pv0;
        for (VLenT j=0; j < nh; ++j, ph += ddh)
        {
            buildAddElem( ph, val );
        }
    }
}

//==================================================================
void Voxel::buildTessTri(
        const Float3 &v0,
        const Float3 &v1,
        const Float3 &v2,
        CellType val )
{
    c_auto mid = (v0 + v1 + v2) * (1.0f/3);
    c_auto a   = (v0 + v1) * 0.5f;
    c_auto b   = (v1 + v2) * 0.5f;
    c_auto c   = (v0 + v2) * 0.5f;

    buildTessQuad( v0, a, c, mid, val );
    buildTessQuad( v1, a, b, mid, val );
    buildTessQuad( v2, b, c, mid, val );
}

//==================================================================
bool Voxel::FindClosestNonEmptyCellCtr(
                        const Float3 &posLS,
                        Float3 &out_foundCellCenterLS ) const
{
    c_auto nn0 = 1 << mN0;
    c_auto nn1 = 1 << mN1;
    c_auto nn2 = 1 << mN2;

    // position to check in Voxel Space
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

                // center of the cell in Voxel Space
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
const VVec<const Voxel::CellType *> &Voxel::CheckLine(
                                            const Float3 &lineSta,
                                            const Float3 &lineEnd ) const
{
    mCheckRes.clear();

    // clip local-space line
    std::array<Float3,2> clipped { lineSta, lineEnd };
    if NOT( clipLineBBox( clipped, &mBBox[0] ) )
        return mCheckRes; // just get out

    // voxel-space line
    Float3 vox[] = {
            (clipped[0] - mBBox[0]) * mVS_LS,
            (clipped[1] - mBBox[0]) * mVS_LS
    };

    c_auto maxVec = Float3( (float)((1 << mN0)-1),
                            (float)((1 << mN1)-1),
                            (float)((1 << mN2)-1) );
    vox[0] = glm::clamp( vox[0], Float3{0,0,0}, maxVec );
    vox[1] = glm::clamp( vox[1], Float3{0,0,0}, maxVec );

    auto diff = vox[1] - vox[0];

    VLenT ia, ta;
    VLenT ib, tb;
    VLenT ic, tc;
#if defined(TEST_WORK)
    VLenT nna, nnb, nnc;
#endif

    c_auto adiff0 = fabs( diff[0] );
    c_auto adiff1 = fabs( diff[1] );
    c_auto adiff2 = fabs( diff[2] );

    if ( adiff0 > adiff1 )
    {
        if ( adiff0 > adiff2 ) {
            ia = 0; ta = 0;
            ib = 1; tb = mN0;
            ic = 2; tc = mN0 + mN1;
#if defined(TEST_WORK)
            nna = 1 << mN0;
            nnb = 1 << mN1;
            nnc = 1 << mN2;
#endif
        } else {
            ia = 2; ta = mN0 + mN1;
            ib = 0; tb = 0;
            ic = 1; tc = mN0;
#if defined(TEST_WORK)
            nna = 1 << mN2;
            nnb = 1 << mN0;
            nnc = 1 << mN1;
#endif
        }
    }
    else
    {
        if ( adiff1 > adiff2 ) {
            ia = 1; ta = mN0;
            ib = 2; tb = mN0 + mN1;
            ic = 0; tc = 0;
#if defined(TEST_WORK)
            nna = 1 << mN1;
            nnb = 1 << mN2;
            nnc = 1 << mN0;
#endif
        } else {
            ia = 2; ta = mN0 + mN1;
            ib = 0; tb = 0;
            ic = 1; tc = mN0;
#if defined(TEST_WORK)
            nna = 1 << mN2;
            nnb = 1 << mN0;
            nnc = 1 << mN1;
#endif
        }
    }

    if ( vox[0][ia] > vox[1][ia] )
    {
        std::swap( vox[0], vox[1] );
        diff = -diff;
    }

    c_auto a0 = (VLenT)vox[0][ia];
    c_auto a1 = (VLenT)vox[1][ia];

    //float fa = vox[0][ia] - (float)a0;

    c_auto lena = a1 - a0 + 1;

    mCheckRes.resize( lena );

    c_auto oolena = 1.f / (float)lena;

    c_auto db = diff[ib] * oolena;
    c_auto dc = diff[ic] * oolena;

    auto b = vox[0][ib];// + fa * db;
    auto c = vox[0][ic];// + fa * dc;
    for (VLenT a=a0; a <= a1; a += 1, b += db, c += dc)
    {
#if defined(TEST_WORK)
        VOXASSERT( (VLenT)a < nna && (VLenT)b < nnb && (VLenT)c < nnc );
#endif
        c_auto a_idx = (size_t)a << ta;
        c_auto b_idx = (size_t)b << tb;
        c_auto c_idx = (size_t)c << tc;
        mCheckRes[a-a0] = &mCells[ a_idx + b_idx + c_idx ];
    }

    return mCheckRes;
}

