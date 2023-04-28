//==================================================================
/// CS_ModelFactory.h
///
/// Created by Davide Pasca - 2023/04/28
/// See the file "license.txt" that comes with this project for
/// copyright info.
//==================================================================

#ifndef CS_MODELFACTORY_H
#define CS_MODELFACTORY_H

#include <string>
#include <vector>
#include <memory>
#include "CS_TrainBase.h"
#include "CS_BrainBase.h"

namespace CS_ModelFactory
{

size_t GetModelsN();

std::string GetModelName(size_t idx);

std::unique_ptr<CS_BrainBase> CreateBrain(
        size_t modelIdx,
        const CS_Chromo& chromo,
        size_t insN, size_t outsN);

std::unique_ptr<CS_TrainBase> CreateTrain(
        size_t modelIdx,
        size_t insN, size_t outsN);

}

#endif

