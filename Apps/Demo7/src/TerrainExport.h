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
        const int quantMaxH,
        const uint32_t cropWH[2] )
{
    std::string str;

    c_auto cropRC = TERR_MakeCropRC( terr.GetSiz(), cropWH );
    c_auto xi1 = cropRC[0];
    c_auto yi1 = cropRC[1];
    c_auto xi2 = cropRC[2];
    c_auto yi2 = cropRC[3];

    str += SSPrintFS( "const unsigned int TERR_WD = %zu;\n", xi2 - xi1 );
    str += SSPrintFS( "const unsigned int TERR_HE = %zu;\n", yi2 - yi1 );
    str += "\n";
    str += "const unsigned char terr_data[TERR_HE][TERR_WD] = {\n";

    float srcMinH =  FLT_MAX;
    float srcMaxH = -FLT_MAX;
    for (size_t yi=yi1; yi < yi2; ++yi)
    {
        c_auto rowCellIdx = yi << terr.GetSizL2();
        for (size_t xi=xi1; xi < xi2; ++xi)
        {
            c_auto cellIdx = xi + rowCellIdx;
            c_auto srcH = terr.mHeights[ cellIdx ];

            srcMinH = std::min( srcMinH, srcH );
            srcMaxH = std::max( srcMaxH, srcH );
        }
    }

    c_auto ooH = 1.f / (srcMaxH - srcMinH);

    size_t dataIdx = 0;
    for (size_t yi=yi1; yi < yi2; ++yi)
    {
        c_auto rowCellIdx = yi << terr.GetSizL2();
        for (size_t xi=xi1; xi < xi2; ++xi)
        {
            c_auto cellIdx = xi + rowCellIdx;

            c_auto srcH = terr.mHeights[ cellIdx ];

            c_auto quantH = std::clamp(
                                (int)(quantMaxH * (srcH - srcMinH) * ooH),
                                0,
                                quantMaxH );

            str += SSPrintFS( "%3i,", quantH );

            if NOT( ++dataIdx & 31 )
                str += "\n";
        }
    }

    str += "\n};\n";

    // write
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

