#include "config.h"
#ifdef ENABLE_MPI
#include <mpi.h>
#endif
#include "SpinModelSampler.h"
#include "action/Action.h"
#include "action/ActionChoice.h"
#include "base/Paths.h"
#include "base/SpinModelState.h"
#include "stats/AccRejEstimator.h"
#include "stats/MPIManager.h"
#include "util/RandomNumGenerator.h"
#include "util/Permutation.h"
#include <sstream>
#include <string>

SpinModelSampler::SpinModelSampler(Paths& paths, Action* action,
        ActionChoiceBase* actionChoice, const MPIManager* mpi) :
        paths(paths), action(action), actionChoice(actionChoice), modelState(
                dynamic_cast<SpinModelState&>(actionChoice->getModelState())), nmodel(
                modelState.getModelCount()), accRejEst(0), mpi(mpi)
#ifdef ENABLE_MPI
        ,nworker((mpi)?mpi->getNWorker():1)
#endif
{
}

SpinModelSampler::~SpinModelSampler() {
}

void SpinModelSampler::run() {
    tryMove();

}

bool SpinModelSampler::tryMove() {
    int workerID = (mpi) ? mpi->getWorkerID() : 0;

    if (workerID == 0)
        accRejEst->tryingMove(0);

    int ipart = nmodel;
    do {
        ipart = int(RandomNumGenerator::getRand() * (nmodel - 1));
//std::cout << ipart << ", " << nmodel << std::endl;
    } while (!(ipart < nmodel - 1));

    // Check if ipart is part of a permutation, reject if yes.
    Permutation permutation = paths.getGlobalPermutation();
    if (ipart != permutation[ipart])
        return false;

    // Evaluate the change in action.
    double deltaAction = actionChoice->getActionChoiceDifference(paths, ipart);

#ifdef ENABLE_MPI
    double totalDeltaAction = 0;
    mpi->getWorkerComm().Reduce(&deltaAction,&totalDeltaAction,
            1,MPI::DOUBLE,MPI::SUM,0);
    deltaAction = totalDeltaAction;
#endif 

    double acceptProb = exp(-deltaAction);

    bool reject = RandomNumGenerator::getRand() > acceptProb;
#ifdef ENABLE_MPI
    if (nworker > 1) {
        mpi->getWorkerComm().Bcast(&reject, sizeof(bool), MPI::CHAR, 0);
    }
#endif 
    if (reject)
        return false;

    modelState.flipSpin(ipart);
#ifdef ENABLE_MPI
    if (mpi) {
//    mpi->getWorkerComm().Bcast(modelState.getModelState().data(),
//                               modelState.getModelCount(),MPI::INT,0);
        modelState.broadcastToMPIWorkers(mpi);
    }
#endif

    if (workerID == 0)
        accRejEst->moveAccepted(0);
    return true;
}

AccRejEstimator*
SpinModelSampler::getAccRejEstimator(const std::string& name) {
    return accRejEst = new AccRejEstimator(name.c_str(), 1);
}
