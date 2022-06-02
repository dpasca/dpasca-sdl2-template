//==================================================================
/// VoxelsGen.h
///
/// Created by Davide Pasca - 2022/06/02
/// See the file "license.txt" that comes with this project for
/// copyright info.
//==================================================================

#ifndef VOXELSGEN_H
#define VOXELSGEN_H

#include "Voxels.h"

//==================================================================
inline void VGen_DrawQuad(
        Voxels &vox,
        const Float3 &p00,
        const Float3 &p01,
        const Float3 &p10,
        const Float3 &p11,
        const Voxels::CellType &val )
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

    c_auto nhf = ceilf( maxh * vox.GetVoxOOUnitForTess() );
    c_auto nvf = ceilf( maxv * vox.GetVoxOOUnitForTess() );
    //VOXASSERT( nhf != 0 && nvf != 0 );

    if ( nhf < 1 || nvf < 1 )
        return;

    c_auto oonhf = 1.f / (nhf - 1);
    c_auto oonvf = 1.f / (nvf - 1);

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
            vox.SetCell( ph, val );
        }
    }
}

//==================================================================
inline void VGen_DrawTrig(
        Voxels &vox,
        const Float3 &v0,
        const Float3 &v1,
        const Float3 &v2,
        const Voxels::CellType &val )
{
    c_auto mid = (v0 + v1 + v2) * (1.0f/3);
    c_auto a   = (v0 + v1) * 0.5f;
    c_auto b   = (v1 + v2) * 0.5f;
    c_auto c   = (v0 + v2) * 0.5f;

    VGen_DrawQuad( vox, v0, a, c, mid, val );
    VGen_DrawQuad( vox, v1, a, b, mid, val );
    VGen_DrawQuad( vox, v2, b, c, mid, val );
}

//==================================================================
inline void VGen_DrawTrigs(
            Voxels &vox,
            const Float3 *pPos,
            const size_t posN,
            const VVec<uint16_t> *pIndices,
            const Voxels::CellType &val )
{
    VOXASSERT( (pIndices && (*pIndices).size() % 3 == 0) || posN % 3 == 0 );

    if ( pIndices )
    {
        c_auto &idxs = *pIndices;
        for (size_t i=0, j=0; i != idxs.size(); i += 3, j += 1)
            VGen_DrawTrig( vox, pPos[idxs[i+0]], pPos[idxs[i+1]], pPos[idxs[i+2]], val );
    }
    else
    {
        for (size_t i=0, j=0; i != posN; i += 3, j += 1)
            VGen_DrawTrig( vox, pPos[i+0], pPos[i+1], pPos[i+2], val );
    }
 }

//==================================================================
inline void VGen_DrawLine(
            Voxels &vox,
            const Float3 &lineSta,
            const Float3 &lineEnd,
            const Voxels::CellType &srcVal )
{
    Voxels_LineScan(
        vox,
        lineSta,
        lineEnd,
        [&]( c_auto ) {},
        [&]( c_auto idx, auto &desVal ){ desVal = srcVal; } );
}

#endif

