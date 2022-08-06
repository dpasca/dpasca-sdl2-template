//==================================================================
/// TerrainExport.h
///
/// Created by Davide Pasca - 2022/06/15
/// See the file "license.txt" that comes with this project for
/// copyright info.
//==================================================================

#ifndef TERRAINEXPORT_H
#define TERRAINEXPORT_H

#include <stdio.h>
#include <stdarg.h>
#include <iostream>
#include <fstream>
#include "Terrain.h"

//==================================================================
static std::string SSPrintFS( const char *pFmt, ... )
{
    char buff[2048] {};
    va_list vl;
    va_start( vl, pFmt );
    vsnprintf( buff, 256, pFmt, vl );
    va_end( vl );

	return {buff};
}

//==================================================================
static void TerrainExport(
        const Terrain &terr,
        const std::string &pathFName,
        const std::string &headStr,
        const int quantMaxH,
        const int quantShade,
        const uint32_t cropWH[2] )
{
    std::string str { headStr + "\n" };

    c_auto cropRC = TERR_MakeCropRC( terr.GetSiz(), cropWH );
    c_auto x1 = cropRC[0];
    c_auto y1 = cropRC[1];
    c_auto x2 = cropRC[2];
    c_auto y2 = cropRC[3];

    auto forEach = [&]( c_auto &fn )
    {
        for (size_t y=y1; y < y2; ++y)
            for (size_t x=x1; x < x2; ++x)
                fn( x + (y << terr.GetSizL2()) );
    };

    //
    str += SSPrintFS( "const unsigned int TERR_WD = %zu;\n", x2 - x1 );
    str += SSPrintFS( "const unsigned int TERR_HE = %zu;\n", y2 - y1 );

    //
    auto quantize = []( float unitVal, int quantVal )
    {
        return (int)std::clamp( quantVal * unitVal, 0.f, (float)quantVal );
    };

    //----------------------------------------
    //--- dump heights
    {
        str += "\n";
        str += "const unsigned char terr_heights[TERR_HE][TERR_WD] = {\n";

        // calc min/max heights to quantize
        float srcMinH =  FLT_MAX;
        float srcMaxH = -FLT_MAX;
        forEach( [&]( c_auto cellIdx )
        {
            c_auto srcH = terr.mHeights[ cellIdx ];

            srcMinH = std::min( srcMinH, srcH );
            srcMaxH = std::max( srcMaxH, srcH );
        });

        // output quantized heights
        c_auto ooH = 1.f / (srcMaxH - srcMinH);
        size_t dataIdx = 0;
        forEach( [&]( c_auto cellIdx )
        {
            c_auto srcH = terr.mHeights[ cellIdx ];

            str += SSPrintFS( "%3i,", quantize( (srcH - srcMinH) * ooH, quantMaxH ) );

            if NOT( ++dataIdx & 31 )
                str += "\n";
        });
        str += "};\n";
    }

    //----------------------------------------
    //--- dump shading
    {
        str += "\n";
        str += "const unsigned char terr_shades[TERR_HE][TERR_WD] = {\n";

        size_t dataIdx = 0;
        forEach( [&]( c_auto cellIdx )
        {
            c_auto dif = terr.mDiffLight[ cellIdx ] / 255.f;
            c_auto sha = terr.mIsShadowed[ cellIdx ] ? 0.f : 1.f;

            str += SSPrintFS( "%3i,", quantize( dif * sha, quantShade ) );

            if NOT( ++dataIdx & 31 )
                str += "\n";
        });
        str += "};\n";
    }

    //----------------------------------------
    //--- dump material
    {
        str += "\n";
        str += "const unsigned char terr_materials[TERR_HE][TERR_WD] = {\n";

        size_t dataIdx = 0;
        forEach( [&]( c_auto cellIdx )
        {
            str += SSPrintFS( "%i,", (int)terr.mMateID[ cellIdx ] );

            if NOT( ++dataIdx & 31 )
                str += "\n";
        });
        str += "};\n";
    }

    //----------------------------------------
    //--- write to file
    std::ofstream file;
    file.open( pathFName );
    if NOT( file.is_open() )
    {
        printf( "** ERROR could not open %s\n", pathFName.c_str() );
        return;
    }

    file << str;
    file.close();

    printf( "** Successfully exported to %s\n", pathFName.c_str() );
}

#endif

