//==================================================================
/// TerrainGen.h
///
/// Created by Davide Pasca - 2022/06/14
/// See the file "license.txt" that comes with this project for
/// copyright info.
//==================================================================

#ifndef TERRAINGEN_H
#define TERRAINGEN_H

#include <vector>
#include <algorithm>
#include "DBase.h"
#include "MathBase.h"
#include "MU_ParallelOcclChecker.h"
#include "Terrain.h"

//==================================================================
static void TGEN_ScaleHeights( auto &terr, float newMin, float newMax )
{
    auto *p = terr.mHeights.data();
    c_auto n = (size_t)1 << ((int)terr.mSizeL2 * 2);

    float mi =  FLT_MAX;
    float ma = -FLT_MAX;
    for (size_t i=0; i < n; ++i)
    {
        mi = std::min( mi, p[i] );
        ma = std::max( ma, p[i] );
    }

    // rescale and offset
    c_auto scaToNew = (ma != mi) ? ((newMax - newMin) / (ma - mi)) : 0.f;
    for (size_t i=0; i < n; ++i)
        p[i] = newMin + (p[i] - mi) * scaToNew;

    // update the terrain values
    terr.mMinH = newMin;
    terr.mMaxH = newMax;
}

//==================================================================
inline auto remapRange( c_auto &v, c_auto &srcL, c_auto &srcR, c_auto &desL, c_auto &desR )
{
    c_auto t = (v - srcL) / (srcR - srcL);
    return glm::mix( desL, desR, t );
}

//==================================================================
static void TGEN_MakeMateAndTex( auto &terr )
{
    for (size_t i=0; i < terr.mHeights.size(); ++i)
    {
        auto &h = terr.mHeights[i];

        // material
        terr.mMateID[ i ] = (h >= 0 ? MATEID_LAND : MATEID_SEA);

        // only build a "texture" for the sea
        terr.mTexMono[ i ] = terr.mMateID[ i ] == MATEID_LAND
                ? 255
                : (uint8_t)remapRange( h, terr.mMinH, 0, 40.f, 255.f );
    }
}

//==================================================================
static void TGEN_FlattenSeaBed( auto &terr )
{
    for (auto &h : terr.mHeights)
        h = std::max( h, 0.f );
}

//==================================================================
// geenrate colors and flatten the heights below sea level
static void TGEN_CalcShadows( auto &terr, Float3 lightDirLS )
{
    lightDirLS = glm::normalize( lightDirLS );

    auto checker = MU_ParallelOcclChecker(
                        terr.mHeights.data(),
                        lightDirLS,
                        terr.mMinH,
                        terr.mMaxH,
                        terr.mSizeL2 );

    c_auto siz = (int)terr.GetSiz();
    size_t cellIdx = 0;
	for (int yi=0; yi < siz; ++yi)
	{
        for (int xi=0; xi < siz; ++xi, ++cellIdx)
		{
            terr.mIsShadowed[cellIdx] = checker.IsOccludedAtPoint( xi, yi );
		}
	}
}

//==================================================================
static void TGEN_CalcDiffLight( auto &terr, Float3 lightDirLS )
{
    lightDirLS = glm::normalize( lightDirLS );
#if 1
    c_auto siz = (int)terr.GetSiz();

    c_auto ix1 = (size_t)0;
    c_auto ix2 = (size_t)siz;
    c_auto iy1 = (size_t)0;
    c_auto iy2 = (size_t)siz;

    c_auto &heights = terr.mHeights;
      auto &diffLight = terr.mDiffLight;

    c_auto &shadows = terr.mIsShadowed;

    c_auto cellUnit = 1.f / siz;
    c_auto y = -2 * cellUnit;
    c_auto ySqrt = y * y;

    for (auto iy=iy1, r00=iy1*siz; iy < iy2; ++iy, r00 += siz)
    {
        c_auto r10 = (iy == siz-1) ? (size_t)0 : r00 + siz;

        for (auto c00=ix1; c00 < ix2; ++c00)
        {
            c_auto c01 = (c00 == siz-1) ? (size_t)0 : c00 + 1;

            c_auto a = heights[r00+c00];    // a----b
            c_auto b = heights[r00+c01];    // |    |
            c_auto c = heights[r10+c00];    // |    |
            c_auto d = heights[r10+c01];    // c----d

            c_auto dh1 = b - a;
            c_auto dv1 = c - a;
            c_auto dh2 = c - d;
            c_auto dv2 = b - d;

            c_auto x = (float)(dh1 - dh2);
            c_auto z = (float)(dv1 - dv2);

            c_auto nOoMag = -1.f / sqrtf( x * x + ySqrt + z * z );

            c_auto nor = nOoMag * Float3( x, y, z );

            c_auto NdotL = glm::dot( nor, lightDirLS );

            diffLight[ r00 + c00 ] =
                (uint8_t)std::clamp( std::max( NdotL * 255.f, 0.f ), 0.f, 255.f );
        }
    }
#endif
}

//==================================================================
static void TGEN_CalcBakedColors( auto &terr, const Float3 &lightDif, const Float3 &amb )
{
    auto makeU8 = []( c_auto valf ) -> std::array<uint8_t,3>
    {
        c_auto valf8 = glm::clamp( 255.f * valf, Float3(0,0,0), Float3(255,255,255) );

        return { (uint8_t)valf8[0],
                 (uint8_t)valf8[1],
                 (uint8_t)valf8[2] };
    };

    for (size_t i=0; i < terr.mHeights.size(); ++i)
    {
        c_auto chr = (terr.mMateID[i] == MATEID_LAND ? CHROM_LAND : CHROM_SEA);
        c_auto tex = (terr.mTexMono[i] * (1.f/255));
        c_auto dif = (terr.mDiffLight[i] * (1.f/255));
        c_auto sha = (terr.mIsShadowed[i] ? 0.0f : 1.f);

        c_auto colU8 = makeU8( chr * tex * (amb + lightDif * dif * sha) );

        // assign the final color
        terr.mBakedCols[i] = { colU8[0], colU8[1], colU8[2], 255 };
    }
}

#endif

