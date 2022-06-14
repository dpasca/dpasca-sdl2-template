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
static void TGEN_CalcShadows( auto &terr, Float3 lightDirWS )
{
    lightDirWS = glm::normalize( lightDirWS );

    auto checker = MU_ParallelOcclChecker(
                        terr.mHeights.data(),
                        lightDirWS,
                        terr.mMinH,
                        terr.mMaxH,
                        terr.mSizeL2 );

    c_auto dim = (int)terr.GetDim();
    size_t cellIdx = 0;
	for (int yi=0; yi < dim; ++yi)
	{
        for (int xi=0; xi < dim; ++xi, ++cellIdx)
		{
            terr.mIsShadowed[cellIdx] = checker.IsOccludedAtPoint( xi, yi );
		}
	}
}

//==================================================================
// geenrate colors and flatten the heights below sea level
static void TGEN_CalcShadeDiff( auto &terr, Float3 lightDirWS )
{
    lightDirWS = glm::normalize( lightDirWS );

    c_auto dim = (int)terr.GetDim();
    size_t cellIdx = 0;
	for (int yi=0; yi < dim; ++yi)
	{
        for (int xi=0; xi < dim; ++xi, ++cellIdx)
		{
            // TODO
		}
	}
}

//==================================================================
static void TGEN_CalcBakedColors( auto &terr )
{
    for (size_t i=0; i < terr.mHeights.size(); ++i)
    {
        c_auto col3f =
            glm::clamp(
                  (terr.mMateID[i] == MATEID_LAND ? CHROM_LAND : CHROM_SEA)
                * (terr.mTexMono[i] * (1.f/255))
                * (terr.mIsShadowed[i] ? 0.5f : 1.f)
                * 255.f,
                Float3( 0,  0,  0   ),
                Float3( 255,255,255 ) );

        // assign the final color
        terr.mBakedCols[i] = { (uint8_t)col3f[0], (uint8_t)col3f[1], (uint8_t)col3f[2], 255 };;
    }
}

#endif

