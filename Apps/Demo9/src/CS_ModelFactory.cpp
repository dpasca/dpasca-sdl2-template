//==================================================================
/// CS_ModelFactory.cpp
///
/// Created by Davide Pasca - 2023/04/28
/// See the file "license.txt" that comes with this project for
/// copyright info.
//==================================================================

#include "CS_M1_Train.h"
#include "CS_ModelFactory.h"

namespace CS_ModelFactory
{

//==================================================================
static std::vector<std::string> _sModelNames = {"Model 1"};

//==================================================================
size_t GetModelsN()
{
    return 1;
}

//==================================================================
std::string GetModelName(size_t idx)
{
    return _sModelNames[idx];
}

//==================================================================
std::unique_ptr<CS_M1_Brain> CreateBrain(
        size_t modelIdx,
        const CS_Chromo& chromo,
        size_t insN, size_t outsN)
{
    switch (modelIdx)
    {
    case 0: return std::make_unique<CS_M1_Brain>(chromo, insN, outsN);
    }
    return {};
}

//==================================================================
std::unique_ptr<CS_TrainBase> CreateTrain(
        size_t modelIdx,
        size_t insN, size_t outsN)
{
    switch (modelIdx)
    {
    case 0: return std::make_unique<CS_M1_Train>(insN, outsN);
    default: throw std::runtime_error("Unknown model index");
    }
}

}
