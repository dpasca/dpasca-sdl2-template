//==================================================================
/// Plasma2.h
///
/// Created by Davide Pasca - 2010/11/23
/// See the file "license.txt" that comes with this project for
/// copyright info.
//==================================================================

#ifndef PLASMA2_H
#define PLASMA2_H

#include <stdint.h>
#include <memory>

class Rand2D;

//==================================================================
class Plasma2
{
    std::vector<float>      mBaseGrid;
    const size_t            BLOCKS_NL2      {0};

    std::unique_ptr<Rand2D> mGen_oRandPool;
    size_t                  mGen_IterIX     {0};
    size_t                  mGen_IterIY     {0};

public:
    struct Params
    {
        float       *pDest      {};
        size_t      sizL2       {8};
        size_t      baseSizL2   {4};
        uint32_t    seed        {};
        float       sca         {1};
        float       rough       {0.5f};
    };
private:
    Params      mPar;

public:
    Plasma2( Params &par );
    ~Plasma2();

    void RendBlock( size_t ix, size_t iy );

    bool IterateBlock();
    bool IterateRow();
};

#endif
