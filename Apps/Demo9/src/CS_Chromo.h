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

//==================================================================
class CS_Chromo
{
    std::vector<uint8_t> mChromoBytes;

public:
    CS_Chromo() = default;

    CS_Chromo CreateEmptyClone() const
    {
        CS_Chromo chromo;
        chromo.mChromoBytes.resize(mChromoBytes.size());
        return chromo;
    }

    template <typename T>
    void SetChromoData(const T* pData, size_t size)
    {
        const auto sizeBytes = size * sizeof(T);
        mChromoBytes.resize(sizeBytes);
        memcpy(mChromoBytes.data(), pData, sizeBytes);
    }

    template <typename T>
    void SetChromoData(const T& data)
    {
        SetChromoData(&data, 1);
    }

    template <typename T>       T* GetChromoData()        { return (T *)mChromoBytes.data(); }
    template <typename T> const T* GetChromoData() const  { return (const T *)mChromoBytes.data(); }

    template <typename T>
    size_t GetChromoDataSize() const
    {
        return mChromoBytes.size() / sizeof(T);
    }

    template <typename T>
    void AppendChromoData(const T* pData, size_t size)
    {
        const auto oldSizeBytes = mChromoBytes.size();
        const auto sizeBytes = size * sizeof(T);
        mChromoBytes.resize(oldSizeBytes + sizeBytes);
        memcpy(mChromoBytes.data() + oldSizeBytes, pData, sizeBytes);
    }

    template <typename T>
    void ReserveChromoData(size_t size)
    {
        const auto sizeBytes = size * sizeof(T);
        mChromoBytes.reserve(sizeBytes);
    }

    // make a C++ std::hash of the vector
    uint64_t ToHash() const
    {
        uint64_t h = 0;
        for (auto& v : mChromoBytes)
            h ^= std::hash<uint8_t>()(v) + 0x9e3779b9 + (h << 6) + (h >> 2);
        return h;
    }
    std::string ToHashHex() const
    {
        std::stringstream ss;
        ss << std::hex << ToHash();
        return ss.str();
    }
};

#endif

