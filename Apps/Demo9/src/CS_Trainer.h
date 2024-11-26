//==================================================================
/// CS_Trainer.h
///
/// Created by Davide Pasca - 2023/04/28
/// See the file "license.txt" that comes with this project for
/// copyright info.
//==================================================================

#ifndef CS_TRAINER
#define CS_TRAINER

#include <future>
#include <atomic>
#include <functional>
#include <vector>
#include <memory>
#include "CS_Brain.h"
#include "CS_Train.h"

//==================================================================
static inline bool isFutureReady( const std::future<void> &f )
{
    return f.wait_for( std::chrono::seconds(0) ) == std::future_status::ready;
}

//==================================================================
class CS_QuickThreadPool
{
    const size_t                   mTheadsN;
    std::vector<std::future<void>> mFutures;

public:
    CS_QuickThreadPool( size_t threadsN )
        : mTheadsN(threadsN)
    {
        mFutures.reserve( threadsN );
    }

    ~CS_QuickThreadPool()
    {
        JoinTheads();
    }

    void JoinTheads()
    {
        try {
            for (auto &f : mFutures)
                if (f.valid())
                    f.get();
        }
        catch(const std::exception& ex)
        {
            printf("ERROR: Uncaught Exception ! '%s'\n", ex.what());
            throw;
        }
    }

    void AddThread( std::function<void ()> fn )
    {
        // flush what's done
        mFutures.erase(
            std::remove_if(
                mFutures.begin(), mFutures.end(),
                [&]( const auto &a ){ return isFutureReady( a ); } ),
            mFutures.end() );

        // force wait if we're full
        while ( mFutures.size() >= mTheadsN )
        {
            mFutures[0].get();
            mFutures.erase( mFutures.begin() );
        }

        mFutures.push_back( std::async( std::launch::async, fn ) );
    }
};

//==================================================================
class CS_Trainer
{
public:
    using CreateBrainFnT      = std::function<std::unique_ptr<CS_Brain>(const CS_Chromo&, size_t, size_t)>;
    using EvalBrainT          = std::function<double (const CS_Brain&, std::atomic<bool>&)>;
    using OnEpochEndFnT       = std::function<std::vector<CS_Chromo>(size_t,const CS_Chromo*,const double*,size_t)>;

private:
    std::future<void>   mFuture;
    std::atomic<bool>   mShutdownReq {};
    size_t              mCurEpochN {};
    std::unique_ptr<CS_Train> moTrain;

public:
    struct Params
    {
        size_t          maxEpochsN {};
        EvalBrainT      evalBrainFn;
    };
public:
    CS_Trainer(const Params& par, std::unique_ptr<CS_Train> &&oTrain)
        : moTrain( std::move(oTrain))
    {
        mFuture = std::async(std::launch::async, [this,par=par](){ ctor_execution(par); });
    }

    void LockViewBestChromos(
            const std::function<void(
                const std::vector<CS_Chromo>&,
                const std::vector<CS_ChromoInfo>&
                )>& func)
    {
        moTrain->LockViewBestChromos(func);
    }

private:
    void ctor_execution(const Params& par)
    {
        // get the starting chromosomes (i.e. random or from file)
        auto chromos = moTrain->MakeStartChromos();
        size_t popN = chromos.size();

        for (size_t eidx=0; eidx < par.maxEpochsN && !mShutdownReq; ++eidx)
        {
            mCurEpochN = eidx;

            // fitnesses are the results of the execution
            std::vector<std::atomic<double>> fitnesses(popN);
            {
                // create a thread for each available core
                CS_QuickThreadPool thpool( std::thread::hardware_concurrency() + 1 );

                // for each member of the population...
                for (size_t pidx=0; pidx < popN && !mShutdownReq; ++pidx)
                {
                    thpool.AddThread([this, &chromo=chromos[pidx], &fitness=fitnesses[pidx], &par]()
                    {
                        // create and evaluate the brain with the given chromosome
                        fitness = par.evalBrainFn(*moTrain->CreateBrain(chromo), mShutdownReq);
                    });
                }
            }

            // if we're shutting down, then exit before calling OnEpochEnd()
            if (mShutdownReq)
                break;

            // generate the new chromosomes
            std::vector<CS_ChromoInfo> infos;
            infos.resize(popN);
            for (size_t pidx=0; pidx < popN; ++pidx)
            {
                auto& ci = infos[pidx];
                ci.ci_fitness = fitnesses[pidx];
                ci.ci_epochIdx = eidx;
                ci.ci_popIdx = pidx;
            }

            chromos = moTrain->OnEpochEnd(eidx, chromos.data(), infos.data(), popN);

            popN = chromos.size();
        }
    }
public:
    auto& GetTrainerFuture() { return mFuture; }

    size_t GetCurEpochN() const { return mCurEpochN; }

    void ReqShutdown() { mShutdownReq = true; }
};

#endif
