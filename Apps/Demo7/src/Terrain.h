//==================================================================
/// Terrain.h
///
/// Created by Davide Pasca - 2022/06/14
/// See the file "license.txt" that comes with this project for
/// copyright info.
//==================================================================

#ifndef TERRAIN_H
#define TERRAIN_H

#include <vector>
#include <array>
#include "DBase.h"
#include "MathBase.h"
#include "RendBase.h"

enum : uint8_t
{
    MATEID_LAND ,
    MATEID_SEA  ,
};

static constexpr auto CHROM_LAND = Float3{ 0.8f , 0.7f , 0.0f }; // chrominance for land
static constexpr auto CHROM_SEA  = Float3{ 0.0f , 0.6f , 0.9f } * 1.5f; // chrominance for sea

//==================================================================
class Terrain
{
public:
    size_t                  mSizeL2 {};
    std::vector<float>      mHeights;
    std::vector<uint8_t>    mTexMono;
    std::vector<uint8_t>    mMateID;
    std::vector<bool>       mIsShadowed;
    std::vector<float>      mDiffLight;
    std::vector<RBColType>  mBakedCols;
    float                   mMinH   {0};
    float                   mMaxH   {1.5f};

    Terrain() {}

    Terrain( size_t sizeL2 )
        : mSizeL2(sizeL2)
        , mHeights( (size_t)1 << ((int)sizeL2 * 2) )
    {
        c_auto n = mHeights.size();
        // initialize with default values
        mTexMono    = std::vector<uint8_t>  ( n, 255 );
        mMateID     = std::vector<uint8_t>  ( n, 0 );
        mIsShadowed = std::vector<bool>     ( n, false );
        mDiffLight  = std::vector<float>    ( n, 1.f );
        mBakedCols  = std::vector<RBColType>( n, RBColType{255,0,255,255} );
    }

    size_t GetSizL2() const { return mSizeL2; }
    size_t GetSiz() const { return (size_t)1 << mSizeL2; }

    //size_t MakeIndexXY( size_t x, size_t y ) const { return x + (y << mSizeL2); }
};

//==================================================================
inline std::array<size_t,4> TERR_MakeCropRC( size_t siz, const uint32_t cropWH[2] )
{
    // define the usable crop area (all if 0, otherwise no larger than the map's siz)
    c_auto useCropW = cropWH[0] ? std::min( (size_t)cropWH[0], siz ) : siz;
    c_auto useCropH = cropWH[1] ? std::min( (size_t)cropWH[1], siz ) : siz;

    return
    {
        (siz - useCropW) / 2, // xi1
        (siz - useCropH) / 2, // yi1
        (siz + useCropW) / 2, // xi2
        (siz + useCropH) / 2, // yi2
    };
}

#endif

