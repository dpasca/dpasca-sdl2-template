//==================================================================
/// CS_TrainBase.h
///
/// Created by Davide Pasca - 2023/04/28
/// See the file "license.txt" that comes with this project for
/// copyright info.
//==================================================================

#ifndef CS_TRAINBASE_H
#define CS_TRAINBASE_H

#include <functional>
#include <vector>
#include <memory>
#include <sstream>
#include "CS_BrainBase.h"

struct CS_ChromoInfo
{
    double    ci_cost {0.0};
    size_t    ci_epochIdx {0};
    size_t    ci_popIdx {0};

    std::string MakeStrID() const
    {
        std::stringstream ss;
        ss << "epoch:" << ci_epochIdx << ",idx:" << ci_popIdx;
        return ss.str();
    }
};

//==================================================================
class CS_TrainBase
{
public:
    const size_t mInsN;
    const size_t mOutsN;

    template <typename T> using function = std::function<T>;
    template <typename T> using vector = std::vector<T>;
    template <typename T> using unique_ptr = std::unique_ptr<T>;

    CS_TrainBase(size_t insN, size_t outsN)
        : mInsN(insN)
        , mOutsN(outsN)
    {
    }

    virtual ~CS_TrainBase() = default;

    virtual unique_ptr<CS_BrainBase> CreateBrain(const CS_Chromo& chromo) = 0;
    virtual vector<CS_Chromo>  MakeStartChromos() = 0;

    virtual vector<CS_Chromo>  OnEpochEnd(
            size_t epochIdx,
            const CS_Chromo* pChromos,
            const CS_ChromoInfo* pInfos,
            size_t n) = 0;

    virtual void LockViewBestChromos(
            const std::function<void(
                const std::vector<CS_Chromo>&,
                const std::vector<CS_ChromoInfo>&
                )>& func) = 0;
};

#endif

