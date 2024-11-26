//==================================================================
/// CS_Chromo.h
///
/// Created by Davide Pasca - 2023/04/28
/// See the file "license.txt" that comes with this project for
/// copyright info.
//==================================================================

#ifndef CS_CHROMO_H
#define CS_CHROMO_H

#include <vector>
#include <string>
#include <sstream>
#include <utility>
#include <memory>
#include <cstring>
#include "CS_Math.h"

//==================================================================
class CS_Chromo
{
public: // For now...
    std::vector<CS_SCALAR> mChromoData;

public:
    CS_Chromo() = default;

    CS_Chromo CreateEmptyClone() const
    {
        CS_Chromo chromo;
        chromo.mChromoData.resize(mChromoData.size());
        return chromo;
    }

    void SetChromoData(const CS_SCALAR* pData, size_t size)
    {
        mChromoData.assign(pData, pData + size);
    }

    void SetChromoData(CS_SCALAR data)
    {
        mChromoData.assign(1, data);
    }

    CS_SCALAR* GetChromoData() { return mChromoData.data(); }
    const CS_SCALAR* GetChromoData() const { return mChromoData.data(); }

    size_t GetSize() const { return mChromoData.size(); }

    void Clear() { mChromoData.clear(); }
    bool IsEmpty() const { return mChromoData.empty(); }

    std::string ToString() const
    {
        std::ostringstream oss;
        for (const auto& val : mChromoData)
            oss << val << " ";
        return oss.str();
    }
};

#endif
