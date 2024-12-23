//==================================================================
/// CS_Brain.cpp
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
#include "CS_Brain.h"

using namespace std;

//==================================================================
template <typename T>
class SimpleNN
{
    static constexpr bool USE_XAVIER_INIT = true;
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
        assert(chromo.GetChromoDataSize() == CalcNNSize(layerNs));

        const auto* ptr = chromo.GetChromoData();
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
        if (USE_XAVIER_INIT)
        {
            // use Xavier initialization
            const T SQRT_2 = (T)std::sqrt(2.0);
            std::normal_distribution<T> dis((T)0.0, (T)1.0 / SQRT_2);

            // initialize weights and biases with random values
            for (auto& l : mLs)
            {
                l.Wei.ForEach([&](auto& x){ x = dis(gen); });
                l.Bia.ForEach([&](auto& x){ x = dis(gen); });
            }
        }
        else
        {
            // use random initialization
            std::uniform_real_distribution<T> dis((T)-1.0, (T)1.0);

            constexpr auto BIAS_SCALE = (T)0.1;

            // initialize weights and biases with random values
            for (auto& l : mLs)
            {
                l.Wei.ForEach([&](auto& x){ x = dis(gen); });
                l.Bia.ForEach([&](auto& x){ x = BIAS_SCALE * dis(gen); });
            }
        }
    }

    // flatten to a chromosome
    CS_Chromo FlattenNN() const
    {
        CS_Chromo chromo;
        chromo.mChromoData.reserve(calcNNSize());
        for (const auto& l : mLs)
        {
            const auto* weiData = l.Wei.data();
            const auto* biaData = l.Bia.data();
            chromo.mChromoData.insert(chromo.mChromoData.end(), weiData, weiData + l.Wei.size());
            chromo.mChromoData.insert(chromo.mChromoData.end(), biaData, biaData + l.Bia.size());
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
        auto activ_vec =
        /* sigm       */ //[](auto& v) { for (auto& x : v) x = T(1.0) / (T(1.0) + exp(-x)); };
        /* tanh       */ //[](auto& v) { for (auto& x : v) x = tanh(x); };
        /* relu       */ //[](auto& v) { for (auto& x : v) x = std::max(T(0), x); };
        /* leaky_relu */ //[](auto& v) { for (auto& x : v) x = std::max(T(0.01)*x, x); };
        /* gelu       */ [](auto& v) { for (auto& x : v) x = x * T(0.5) * (T(1.0) + erf(x / sqrt(T(2.0)))); };

        //auto activ_vec = gelu_vec;

        auto* pTempMem0 = (T*)alloca(mMaxLenVecN * sizeof(T));
        auto* pTempMem1 = (T*)alloca(mMaxLenVecN * sizeof(T));

        {
            Vec tmp0(pTempMem0, mLs[0].Wei.size_cols());
            CSM_Vec_mul_Mat(tmp0, ins, mLs[0].Wei);
            tmp0 += mLs[0].Bia;
            activ_vec(tmp0);
        }
        for (size_t i=1; i < mLs.size()-1; ++i)
        {
            const auto& l = mLs[i];
            Vec tmp0(pTempMem0, mLs[i-1].Wei.size_cols());
            Vec tmp1(pTempMem1, l.Wei.size_cols());
            CSM_Vec_mul_Mat(tmp1, tmp0, l.Wei);
            tmp1 += l.Bia;
            activ_vec(tmp1);
            std::swap(pTempMem0, pTempMem1);
        }
        {
            const auto& l = mLs.back();
            Vec tmp0(pTempMem0, mLs[mLs.size()-2].Wei.size_cols());
            CSM_Vec_mul_Mat(outs, tmp0, l.Wei);
            outs += l.Bia;
            activ_vec(outs);
        }
    }
};

template class SimpleNN<CS_SCALAR>;

//==================================================================
static std::vector<size_t> makeLayerNs(size_t insN, size_t outsN)
{
    return std::vector<size_t>{
        insN,
        std::max((size_t)(insN * 1.25), outsN),
        std::max((size_t)(insN * 0.75), outsN),
        std::max((size_t)(insN * 0.25), outsN),
        outsN};
}

//==================================================================
CS_Brain::CS_Brain(const CS_Chromo& chromo, size_t insN, size_t outsN)
{
    const auto layerNs = makeLayerNs(insN, outsN);
    moNN = std::make_unique<SimpleNN<CS_SCALAR>>(chromo, layerNs);
}
//
CS_Brain::CS_Brain(uint32_t seed, size_t insN, size_t outsN)
{
    const auto layerNs = makeLayerNs(insN, outsN);
    moNN = std::make_unique<SimpleNN<CS_SCALAR>>(seed, layerNs);
}

//
CS_Brain::~CS_Brain() = default;

//==================================================================
CS_Chromo CS_Brain::MakeBrainChromo() const
{
    return moNN->FlattenNN();
}

//==================================================================
void CS_Brain::AnimateBrain(const CSM_Vec& ins, CSM_Vec& outs) const
{
    moNN->ForwardPass(outs, ins);
}
