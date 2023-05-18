//==================================================================
/// CS_M1_Train.h
///
/// Created by Davide Pasca - 2023/04/28
/// See the file "license.txt" that comes with this project for
/// copyright info.
//==================================================================

#ifndef CS_M1_TRAIN_H
#define CS_M1_TRAIN_H

#include <functional>
#include <vector>
#include <memory>
#include <mutex>
#include <random>
#include "CS_TrainBase.h"
#include "CS_M1_Types.h"
#include "CS_M1_Brain.h"

//==================================================================
static auto uniformCrossOver = [](auto& rng, const auto& a, const auto& b)
{
    using T = CS_M1_ChromoScalar;

    auto res = a.CreateEmptyClone();
    auto* pRes =  res. template GetChromoData<T>();
    const auto* pA = a. template GetChromoData<T>();
    const auto* pB = b. template GetChromoData<T>();
    const auto n = a. template GetChromoDataSize<T>();
    std::uniform_real_distribution<double> uni(0.0, 1.0);
    for (size_t i=0; i < n; ++i)
        pRes[i] = uni(rng) < 0.5 ? pA[i] : pB[i];

    return res;
};
#if 0
//
static auto singlePointCrossOver = [](auto& rng, auto dist, const auto& a, const auto& b)
{
    using T = CS_M1_ChromoScalar;
    T res(a.size());
    const auto split = std::min( (size_t)(dist(rng) * a.size()), a.size()-1 );

    for (size_t i=0;     i < split;    ++i) res[i] = a[i];
    for (size_t i=split; i < a.size(); ++i) res[i] = b[i];

    return res;
};
//
static auto hybridCrossOver = [](auto& rng, auto&dist, const auto& a, const auto& b)
{
    const auto intermediate = uniformCrossOver(rng, a, b);
    return singlePointCrossOver(rng, dist, intermediate, a);
};
#endif
static auto calcMeanAndStddev = [](const auto& vec)
{
    float sum = 0.0f;
    float sum_squared = 0.0f;
    const auto* p = vec. template GetChromoData<CS_M1_ChromoScalar>();
    const auto n = vec. template GetChromoDataSize<CS_M1_ChromoScalar>();
    for (size_t i=0; i < n; ++i)
    {
        const auto x = p[i];
        sum += x;
        sum_squared += x * x;
    }
    const auto mean = sum / n;
    const auto dcar = (sum_squared / n) - (mean * mean);
    const auto std_dev = sqrt(dcar);

    return std::make_pair(mean, std_dev);
};

static auto mutateNormalDist = [](auto& rng, const auto& vec, float rate)
{
    auto newVec = vec;
    const auto [mean, stddev] = calcMeanAndStddev(vec);
    std::normal_distribution<float> nor(mean, stddev);
    std::uniform_real_distribution<float> uni(0.0, 1.0);
    auto* p = newVec. template GetChromoData<CS_M1_ChromoScalar>();
    const auto n = newVec. template GetChromoDataSize<CS_M1_ChromoScalar>();
    for (size_t i=0; i < n; ++i)
    {
        if (uni(rng) < rate)
            p[i] += (CS_SCALAR)nor(rng);
    }
    return newVec;
};
static auto mutateScaled = [](auto& rng, const auto& vec, float rate)
{
    auto newVec = vec;
    double absSum = 0;

    auto* p = newVec. template GetChromoData<CS_M1_ChromoScalar>();
    const auto n = newVec. template GetChromoDataSize<CS_M1_ChromoScalar>();
    for (size_t i=0; i < n; ++i)
        absSum += std::abs(p[i]);

    const auto avg = (CS_SCALAR)(absSum / (double)n);

    const auto useSca = std::max( (CS_SCALAR)1.0, avg );

    std::uniform_real_distribution<float> uni(0.0, 1.0);
    for (size_t i=0; i < n; ++i)
    {
        if (uni(rng) < rate)
            p[i] += (CS_SCALAR)((uni(rng) * 2 - 1) * useSca);
    }
    return newVec;
};

//==================================================================
//==================================================================
class CS_M1_Train : public CS_TrainBase
{
    static constexpr size_t INIT_POP_N          = 100;
    static constexpr size_t TOP_FOR_SELECTION_N = 10;
    static constexpr size_t TOP_FOR_REPORT_N    = 10;

    // best chromos list just for display
    std::mutex                 mBestChromosMutex;
	std::vector<CS_Chromo>     mBestChromos;
    std::vector<CS_ChromoInfo> mBestCInfos;

public:
    CS_M1_Train(size_t insN, size_t outsN)
        : CS_TrainBase(insN, outsN)
    {
    }

    ~CS_M1_Train() override = default;

    //==================================================================
    unique_ptr<CS_BrainBase> CreateBrain(const CS_Chromo &chromo) override
    {
        return std::make_unique<CS_M1_Brain>(chromo, mInsN, mOutsN);
    }

    //==================================================================
    // initial list of chromosomes
    vector<CS_Chromo>  MakeStartChromos() override
    {
        std::vector<CS_Chromo> chromos;
        for (size_t i=0; i < INIT_POP_N; ++i)
        {
            // make a temp brain from a random seed
            CS_M1_Brain brain((uint32_t)i, mInsN, mOutsN);
            // store the brain's chromo
            chromos.push_back( brain.MakeBrainChromo() );
        }
        return chromos;
    }
    //==================================================================
    // when an epoch has ended
    vector<CS_Chromo>  OnEpochEnd(
            size_t epochIdx,
            const CS_Chromo* pChromos,
            const CS_ChromoInfo* pInfos,
            size_t n) override
    {
        // sort by the cost
        std::vector<std::pair<const CS_Chromo*, const CS_ChromoInfo*>> pSorted;
        for (size_t i=0; i < n; ++i)
            pSorted.push_back({ pChromos + i, pInfos + i });

        std::sort(pSorted.begin(), pSorted.end(), [](const auto& a, const auto& b)
        {
            return a.second->ci_fitness > b.second->ci_fitness;
        });

        // update the list of best chromosomes (with a lock... we're in a different thread)
        updateBestChromosList(pSorted);

        // random generator
        const auto seed = (unsigned int)epochIdx;
        std::mt19937 rng(seed);
        std::uniform_real_distribution<double> dist(0.0, 1.0);

        // mutation function
        auto mutateChromo = [&](const CS_Chromo &chromo)
        {
            //return mutateScaled(rng, chromo, (CS_SCALAR)0.2);
            return mutateNormalDist(rng, chromo, (CS_SCALAR)0.1);
        };

        std::vector<CS_Chromo> newChromos;
        // breed the top N among each other with some mutations
        for (size_t i=0; i < TOP_FOR_SELECTION_N; ++i)
        {
            const auto& c_i = *pSorted[i].first;
            for (size_t j=i+2; j < TOP_FOR_SELECTION_N; ++j)
            {
                const auto& c_j = *pSorted[j].first;
                newChromos.push_back(              uniformCrossOver(rng, c_i, c_j) );
                newChromos.push_back( mutateChromo(uniformCrossOver(rng, c_i, c_j)) );
                const auto& c_k = *pSorted[j+1].first;
                newChromos.push_back(              uniformCrossOver(rng, c_i, c_k) );
                newChromos.push_back( mutateChromo(uniformCrossOver(rng, c_i, c_k)) );
            }
        }

        return newChromos;
    }

    //==================================================================
    void LockViewBestChromos(
            const std::function<void(
                const std::vector<CS_Chromo>&,
                const std::vector<CS_ChromoInfo>&
                )>& func) override
    {
        std::lock_guard<std::mutex> lock(mBestChromosMutex);
        func(mBestChromos, mBestCInfos);
    }

private:
    //==================================================================
    void updateBestChromosList(
            const std::vector<std::pair<const CS_Chromo*, const CS_ChromoInfo*>>& pSorted)
    {
        std::lock_guard<std::mutex> lock(mBestChromosMutex);

        const auto n = std::min(TOP_FOR_REPORT_N, pSorted.size());

        // append the new best chromos to the list
        for (size_t i=0; i < pSorted.size(); ++i)
        {
            const auto cost = pSorted[i].second->ci_fitness;

            // find the insertion point in the mBestCInfos list
            auto it = std::lower_bound(mBestCInfos.begin(), mBestCInfos.end(), cost,
                [](const auto& a, const auto& b) { return a.ci_fitness > b; });

            // insert at the given index
            if (const auto idx = (size_t)(it - mBestCInfos.begin());
                           idx < TOP_FOR_REPORT_N)
            {
                mBestChromos.insert(mBestChromos.begin() + (ptrdiff_t)idx, *pSorted[i].first);
                mBestCInfos.insert(mBestCInfos.begin() + (ptrdiff_t)idx, *pSorted[i].second);
            }
        }
        mBestChromos.resize(n);
        mBestCInfos.resize(n);
#ifdef DEBUG // verify that they are all sorted
        for (size_t i=1; i < mBestCInfos.size(); ++i)
            assert(mBestCInfos[i-1].ci_fitness >= mBestCInfos[i].ci_fitness);
#endif
    }
};

#endif

