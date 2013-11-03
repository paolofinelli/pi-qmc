#include "config.h"
#ifdef ENABLE_MPI
#include <mpi.h>
#endif
#include "DisplaceMoveSampler.h"
#include "PermutationChooser.h"
#include "ParticleChooser.h"
#include "SimpleParticleChooser.h"
#include "UniformMover.h"
#include "action/Action.h"
#include "base/Paths.h"
#include "stats/MPIManager.h"
#include "stats/AccRejEstimator.h"
#include "util/Permutation.h"
#include "util/RandomNumGenerator.h"
#include "util/SuperCell.h"
#include <sstream>
#include <string>

DisplaceMoveSampler::DisplaceMoveSampler(int nmoving, int nrepeat, Paths& paths,
        ParticleChooser& particleChooser, const UniformMover& mover,
        Action* action, const MPIManager* mpi) :
        nmoving(nmoving), nrepeat(nrepeat), npart(paths.getNPart()), iFirstSlice(
                paths.getLowestOwnedSlice(false)), iLastSlice(
                paths.getHighestOwnedSlice(false)), displacement(nmoving), movingIndex(
                nmoving), particleChooser(particleChooser), mover(mover), paths(
                paths), cell(paths.getSuperCell()), action(action), accRejEst(
                0), mpi(mpi), nworker((mpi) ? mpi->getNWorker() : 1), iworkerPerm(
                (mpi) ? mpi->getNWorker() : 1, paths.getNPart()) {
    std::cout << "In DisplaceMove iFirstSlice=" << iFirstSlice << std::endl;
    std::cout << "In DisplaceMove iLastSlice=" << iLastSlice << std::endl;
}

DisplaceMoveSampler::~DisplaceMoveSampler() {
    delete &particleChooser;
}

Permutation DisplaceMoveSampler::getGlobalPermutation() {

    Permutation globalPermutation = paths.getPermutation();
#ifdef ENABLE_MPI
    if (nworker==1) return globalPermutation;

    int iworker = mpi->getWorkerID();
    Permutation localPermutation = paths.getPermutation();
    for (int i=0;i<npart;i++) iworkerPerm(iworker,i)=localPermutation[i];

    Permutation recvp(npart);
    if (iworker ==0) {
        for (int src =1; src < nworker; ++src) {

            mpi->getWorkerComm().Recv(&recvp[0], npart,MPI::INT,src,1);
            globalPermutation.append(recvp);
            for (int i=0;i<npart;i++) iworkerPerm(src,i)=recvp[i];
        }
    } else {
        mpi->getWorkerComm().Send(&(localPermutation[0]), npart,MPI::INT,0,1);
    }
#endif
    return globalPermutation;
}

void DisplaceMoveSampler::run() {
#ifdef ENABLE_MPI
    int workerID = (mpi)?mpi->getWorkerID():0;
#endif

    for (int irepeat = 0; irepeat < nrepeat; ++irepeat) {

        // Select particles at random and reject move if any of the selected
        // particles are permuting.
        bool reject = false;
        particleChooser.chooseParticles();
        Permutation permutation = getGlobalPermutation();
        for (int i = 0; i < nmoving; ++i) {
            int j = particleChooser[i];
            if (j == permutation[j]) {
                movingIndex(i) = j;
            } else {
                reject = true;
            }
        }

#ifdef ENABLE_MPI
        if (nworker > 1) {
            mpi->getWorkerComm().Bcast(&reject, sizeof(bool), MPI::CHAR, 0);
        }
#endif 
        if (reject)
            continue;
#ifdef ENABLE_MPI
        if (nworker>1) {
            IArray iworkerMovingIndex(nmoving);
            if (workerID==0) {

                for (int i=0; i<nmoving; ++i)
                iworkerMovingIndex(i)=iworkerPerm(0,movingIndex(i));

                mpi->getWorkerComm().Send(&(iworkerMovingIndex)(0), nmoving, MPI::INT, 1, 111);

                for (int iw=2; iw<nworker; iw++) {
                    for (int i=0; i<nmoving; ++i)
                    iworkerMovingIndex(i)=iworkerPerm(iw-1,iworkerMovingIndex(i));
                    mpi->getWorkerComm().Send(&(iworkerMovingIndex)(0), nmoving, MPI::INT, iw, 111);
                }
            } else {
                mpi->getWorkerComm().Recv(&(movingIndex)(0), nmoving, MPI::INT, 0, 111);
            }
        }

#endif

        ////////////////// parallel  db
        /*int ifirst=paths.getLowestOwnedSlice(false)-1;//getNProcSlice();
         int nprocSlice = paths.getHighestOwnedSlice(false)-ifirst;
         if (mpi->getCloneID()==0)
         for (int islice=0; islice<=nprocSlice+1; ++islice) {
         std :: cout << "IW="<<(mpi->getWorkerID())<< ", islice="<<islice+ifirst << ", " << paths(0,islice) << std::endl;
         }*/
        ////////////////////////////////////////////////////////////////////////////////////////
        ////////////////// serial  db
        /* if (movingIndex(0)!=0) continue;// move only first partivcle
         if (mpi->getCloneID()==0){
         std :: cout <<"\n\n\n Start\n ";
         for (int islice=iFirstSlice-1; islice<=iLastSlice; ++islice) {
         std :: cout << "IW="<<(mpi->getWorkerID())<< ", islice="<<islice<< ", " << paths(movingIndex(0),islice) << std::endl;
         }
         }
         */
        ///////////////////////////////////////////////////////////////////////////////////////
        tryMove();

    }
}

void DisplaceMoveSampler::handleBoundary(int bd1, int bd2, int sign) {
    int islice = bd1;
    for (int imoving = 0; imoving < nmoving; ++imoving) {
        Vec &bead(paths(movingIndex(imoving), islice));
        if (sign == 1)
            bead += displacement(imoving);
        else
            bead -= displacement(imoving);
        bead = cell.pbc(bead);
    }

    islice = bd2;
    for (int imoving = 0; imoving < nmoving; ++imoving) {
        Vec &bead(paths(movingIndex(imoving), islice));
        if (sign == 1)
            bead += displacement(imoving);
        else
            bead -= displacement(imoving);
        bead = cell.pbc(bead);
    }
}

bool DisplaceMoveSampler::tryMove() {

    int workerID = (mpi) ? mpi->getWorkerID() : 0;

    if (workerID == 0)
        accRejEst->tryingMove(0);
    double logTranProb = mover.makeMove(displacement, movingIndex);
    if (nworker > 1)
        handleBoundary(iFirstSlice - 1, iLastSlice + 1, +1);

    // Evaluate the change in action.
    double deltaAction =
            (action == 0) ?
                    0 :
                    action->getActionDifference(paths, displacement, nmoving,
                            movingIndex, iFirstSlice, iLastSlice);

#ifdef ENABLE_MPI
    if (mpi && (mpi->getNWorker())>1) {
        double netDeltaAction=0;
        mpi->getWorkerComm().Reduce(&deltaAction,&netDeltaAction,
                1,MPI::DOUBLE,MPI::SUM,0);
        double acceptProb = exp(-netDeltaAction);
        bool acceptReject = RandomNumGenerator::getRand()>acceptProb;
        mpi->getWorkerComm().Bcast(&acceptReject,1,MPI::CHAR,0);
        if (acceptReject) {if (nworker>1) handleBoundary(iFirstSlice-1, iLastSlice+1, -1);return false;}

    } else {
        double acceptProb=exp(-deltaAction+logTranProb);
        if (RandomNumGenerator::getRand()>acceptProb) {if (nworker>1) handleBoundary(iFirstSlice-1, iLastSlice+1, -1);return false;}
    }
#else
    double acceptProb = exp(-deltaAction + logTranProb);
    if (RandomNumGenerator::getRand() > acceptProb) {
        if (nworker > 1)
            handleBoundary(iFirstSlice - 1, iLastSlice + 1, -1);
        return false;
    }
#endif

    if (workerID == 0)
        accRejEst->moveAccepted(0);

    // Move accepted.
    action->acceptLastMove();

    // Put moved beads in paths beads.
    for (int imoving = 0; imoving < nmoving; ++imoving) {
        for (int islice = iFirstSlice; islice <= iLastSlice; ++islice) {
            Vec &bead(paths(movingIndex(imoving), islice));
            bead += displacement(imoving);
            bead = cell.pbc(bead);
        }
    }
    ////////////////// serial  db 
    /*if (mpi->getCloneID()==0) {
     std :: cout <<"\n\n displacement amount for first mover"<< displacement(0)<<std::endl;
     for (int islice=iFirstSlice-1; islice<=iLastSlice; ++islice) {
     std :: cout << "IW="<<(mpi->getWorkerID())<< ", islice="<<islice<< ", " << paths(movingIndex(0),islice) << std::endl;
     }
     }
     std :: cout <<"\n End"; exit(0);
     */
    ////////////////////////////////////////////////////////////////////////////////////////
    // paths.setBuffers();// not really needed
    return true;
}

AccRejEstimator*
DisplaceMoveSampler::getAccRejEstimator(const std::string& name) {
    std::ostringstream longName;
    longName << name << ": moving " << nmoving << " "
            << particleChooser.getName();
    return accRejEst = new AccRejEstimator(longName.str(), 1);
}
