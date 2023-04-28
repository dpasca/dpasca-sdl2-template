//==================================================================
/// CS_M1_Brain.cpp
///
/// Created by Davide Pasca - 2023/04/28
/// See the file "license.txt" that comes with this project for
/// copyright info.
//==================================================================

#include <iostream>
#include <vector>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <cassert>
#include <random>
#include <numeric>

#include "CS_Math.h"
#include "CS_M1_Types.h"
#include "CS_M1_Brain.h"

using namespace std;

//==================================================================
static inline size_t calcHiddenN1(size_t insN, size_t outsN)
{
    return ((insN + outsN) + 1) / 2;
}
static inline size_t calcHiddenN2(size_t insN, size_t outsN)
{
    return std::max( ((insN + outsN) + 3) / 4, outsN+1 );
}

//==================================================================
template <typename T>
class SimpleNN
{
public:
    using Vec = CSM_VecT<T>;
    using Mat = CSM_MatT<T>;
private:
    struct Layer
    {
        Mat Wei;
        Vec Bia;
    };
    std::vector<Layer> mLs;
    size_t mMaxLenVecN {};

public:
    SimpleNN(const std::vector<size_t>& layerNs)
        : mLs(layerNs.size()-1)
    {
        for (size_t i=0; i < layerNs.size()-1; ++i)
        {
            mLs[i].Wei = Mat(layerNs[i], layerNs[i+1]);
            mLs[i].Bia = Vec(layerNs[i+1]);
        }

        mMaxLenVecN = *std::max_element(layerNs.begin(), layerNs.end());
    }

    // create from chromosome
    SimpleNN(const CS_Chromo& chromo, const std::vector<size_t>& layerNs)
        : SimpleNN(layerNs)
    {
        assert(chromo.GetChromoDataSize<CS_M1_ChromoScalar>() == CalcNNSize(layerNs));

        const auto* ptr = chromo.GetChromoData<CS_M1_ChromoScalar>();
        for (auto& l : mLs)
        {
            l.Wei.LoadFromMem(ptr); ptr += l.Wei.size();
            l.Bia.LoadFromMem(ptr); ptr += l.Bia.size();
        }
    }

    // create from random seed
    SimpleNN(uint32_t seed, const std::vector<size_t>& layerNs)
        : SimpleNN(layerNs)
    {
        std::random_device rd;
        std::mt19937 gen( seed ? seed : rd() );
        std::uniform_real_distribution<T> dis((T)-1.0, (T)1.0);

        constexpr auto BIAS_SCALE = (T)0.1;

        // initialize weights and biases with random values
        for (auto& l : mLs)
        {
            l.Wei.ForEach([&](auto& x){ x = dis(gen); });
            l.Bia.ForEach([&](auto& x){ x = BIAS_SCALE * dis(gen); });
        }
    }

    // flatten to a chromosome
    CS_Chromo FlattenNN() const
    {
        CS_Chromo chromo;
        chromo.ReserveChromoData<CS_M1_ChromoScalar>(calcNNSize());
        for (const auto& l : mLs)
        {
            chromo.AppendChromoData(l.Wei.data(), l.Wei.size());
            chromo.AppendChromoData(l.Bia.data(), l.Bia.size());
        }
        return chromo;
    }

    static size_t CalcNNSize(const std::vector<size_t>& layerNs)
    {
        size_t size = 0;
        for (size_t i=0; i < layerNs.size()-1; ++i)
            size += layerNs[i] * layerNs[i+1] + layerNs[i+1];
        return size;
    }
private:
    size_t calcNNSize() const
    {
        return std::accumulate(mLs.begin(), mLs.end(), (size_t)0,
            [](size_t sum, const Layer& l){ return sum + l.Wei.size() + l.Bia.size(); });
    }
public:
    void ForwardPass(Vec& outs, const Vec& ins)
    {
        assert(ins.size()  == mLs[0].Wei.size_rows() &&
               outs.size() == mLs.back().Wei.size_cols());

        // define the activation function
        //auto sigm_vec = [](auto& v) { for (auto& x : v) x = T(1.0) / (T(1.0) + exp(-x)); };
        //auto tanh_vec = [](auto& v) { for (auto& x : v) x = tanh(x); };
        auto relu_vec = [](auto& v) { for (auto& x : v) x = std::max(T(0), x); };
        //auto leaky_relu_vec = [](auto& v) { for (auto& x : v) x = std::max(T(0.01)*x, x); };

        auto* pTempMem0 = (T*)alloca(mMaxLenVecN * sizeof(T));
        auto* pTempMem1 = (T*)alloca(mMaxLenVecN * sizeof(T));

        {
            Vec tmp0(pTempMem0, mLs[0].Wei.size_cols());
            CSM_Vec_mul_Mat(tmp0, ins, mLs[0].Wei);
            tmp0 += mLs[0].Bia;
            relu_vec(tmp0);
        }
        for (size_t i=1; i < mLs.size()-1; ++i)
        {
            const auto& l = mLs[i];
            Vec tmp0(pTempMem0, mLs[i-1].Wei.size_cols());
            Vec tmp1(pTempMem1, l.Wei.size_cols());
            CSM_Vec_mul_Mat(tmp1, tmp0, l.Wei);
            tmp1 += l.Bia;
            relu_vec(tmp1);
            std::swap(pTempMem0, pTempMem1);
        }
        {
            const auto& l = mLs.back();
            Vec tmp0(pTempMem0, mLs[mLs.size()-2].Wei.size_cols());
            CSM_Vec_mul_Mat(outs, tmp0, l.Wei);
            outs += l.Bia;
            relu_vec(outs);
        }
    }
};

template class SimpleNN<CS_SCALAR>;

//==================================================================
CS_M1_Brain::CS_M1_Brain(const CS_Chromo& chromo, size_t insN, size_t outsN)
    : CS_BrainBase(chromo, insN, outsN)
{
    const size_t hid1N = calcHiddenN1(insN, outsN);
    const size_t hid2N = calcHiddenN2(insN, outsN);
    moNN = std::make_unique<SimpleNN<CS_SCALAR>>(chromo, std::vector<size_t>{insN, hid1N, hid2N, outsN});
}
//
CS_M1_Brain::CS_M1_Brain(uint32_t seed, size_t insN, size_t outsN)
    : CS_BrainBase(seed, insN, outsN)
{
    const size_t hid1N = calcHiddenN1(insN, outsN);
    const size_t hid2N = calcHiddenN2(insN, outsN);
    moNN = std::make_unique<SimpleNN<CS_SCALAR>>(seed, std::vector<size_t>{insN, hid1N, hid2N, outsN});
}

//
CS_M1_Brain::~CS_M1_Brain() = default;

//==================================================================
CS_Chromo CS_M1_Brain::MakeBrainChromo() const
{
    return moNN->FlattenNN();
}

//==================================================================
void CS_M1_Brain::AnimateBrain(const CSM_Vec& ins, CSM_Vec& outs) const
{
    moNN->ForwardPass(outs, ins);
}

