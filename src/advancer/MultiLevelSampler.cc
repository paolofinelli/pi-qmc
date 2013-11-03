#include "config.h"
#ifdef ENABLE_MPI
#include <mpi.h>
#endif
#include "MultiLevelSampler.h"
#include "PermutationChooser.h"
#include "ParticleChooser.h"
#include "RandomPermutationChooser.h"
#include "SimpleParticleChooser.h"
#include "SectionChooser.h"
#include "action/Action.h"
#include "base/Beads.h"
#include "base/BeadFactory.h"
#include "base/Paths.h"
#include "mover/Mover.h"
#include "util/RandomNumGenerator.h"
#include "util/Permutation.h"
#include "stats/AccRejEstimator.h"
#include <cstdlib>
#include <sstream>
#include <string>

MultiLevelSampler::MultiLevelSampler(int nmoving, Paths& paths,
        SectionChooser &sectionChooser, ParticleChooser* particleChooser,
        PermutationChooser* permutationChooser, Mover& mover, Action* action,
        const int nrepeat, const BeadFactory& beadFactory,
        const bool delayedRejection, const double defaultFactor,
        double newFactor, bool shouldDeletePermutationChooser) :
    nlevel(sectionChooser.getNLevel()),
    nmoving(nmoving),
    sectionBeads(&sectionChooser.getBeads()),
    sectionPermutation(&sectionChooser.getPermutation()),
    movingBeads(beadFactory.getNewBeads(nmoving, sectionBeads->getNSlice())),
    mover(mover),
    cell(paths.getSuperCell()),
    action(action),
    movingIndex(new IArray(nmoving)),
    identityIndex(nmoving),
    pMovingIndex(nmoving),
    particleChooser(particleChooser),
    permutationChooser(permutationChooser),
    sectionChooser(sectionChooser),
    paths(paths),
    accRejEst(0),
    nrepeat(nrepeat),
    delayedRejection(delayedRejection),
    rejectedBeads((delayedRejection) ?
            beadFactory.getNewBeads(nmoving, sectionBeads->getNSlice()) :
            0),
    newFactor(newFactor),
    defaultFactor(defaultFactor),
    shouldDeletePermutationChooser(shouldDeletePermutationChooser) {
    for (int i = 0; i < nmoving; ++i)
        (*movingIndex)(i) = identityIndex(i) = i;
    factor = 1;
}

MultiLevelSampler::~MultiLevelSampler() {
    delete movingBeads;
    delete rejectedBeads;
    delete movingIndex;
    if (shouldDeletePermutationChooser) {
        delete permutationChooser;
    }
    delete particleChooser;
}

void MultiLevelSampler::run() {
    // Select particles to move and the permuation.
    permutationChooser->init();
    for (int irepeat = 0; irepeat < nrepeat; ++irepeat) {
        bool isNewPerm = permutationChooser->choosePermutation();
        if (isNewPerm) {
            Permutation permutation(permutationChooser->getPermutation());
            particleChooser->chooseParticles();
            double lnTranProb = permutationChooser->getLnTranProb();
            for (int i = 0; i < nmoving; ++i)
                (*movingIndex)(i) = (*particleChooser)[i];
            // Copy old coordinate endpoint to the moving coordinate endpoints.
            const int nsectionSlice = movingBeads->getNSlice();
            for (int imoving = 0; imoving < nmoving; ++imoving) {
                pMovingIndex(imoving) = (*movingIndex)(permutation[imoving]);
            }

            sectionBeads->copySlice(*movingIndex, 0, *movingBeads,
                    identityIndex, 0);
            sectionBeads->copySlice(pMovingIndex, nsectionSlice - 1,
                    *movingBeads, identityIndex, nsectionSlice - 1);
//        (*movingBeads)(imoving,0)=(*sectionBeads)(particleChooser[imoving],0);
//        (*movingBeads)(imoving,nsectionSlice-1)
//          =(*sectionBeads)(particleChooser[permutation[imoving] ],
//                        nsectionSlice-1);
//      }
            if (tryMove(lnTranProb) && irepeat < nrepeat - 1)
                permutationChooser->init();
        }
    }
}

bool MultiLevelSampler::tryMove(double initialLnTranProb) {
    double oldDeltaAction = initialLnTranProb;
    for (int ilevel = nlevel; ilevel >= 0; --ilevel) {
        if (accRejEst)
            accRejEst->tryingMove(ilevel);

        // Make the trial move for this level A2B, and get the acc ratio and transition prob. A2B.
        factor = defaultFactor;
        double lnTranProb = mover.makeMove(*this, ilevel);
        double deltaAction =
                (action == 0) ? 0 : action->getActionDifference(*this, ilevel);
        double piRatioBoA = -deltaAction + oldDeltaAction;
        double acceptProb = exp(lnTranProb + piRatioBoA);

        // If you want to do DelayedRejection
        if (delayedRejection && ilevel == nlevel - 1) {
            if (RandomNumGenerator::getRand() > acceptProb) {
                double accRatioA2B = acceptProb;
                double transA2B = mover.getForwardProb();
                for (int i = 0; i < nmoving; ++i) {
                    for (int islice = 0; islice < sectionBeads->getNSlice();
                            ++islice) {
                        (*rejectedBeads)(i, islice) = (*movingBeads)(i, islice);
                    }
                }

                //Make another trial move for this level A2C, and get the acc ratio.
                factor = newFactor;
                double lnTranProb = mover.makeMove(*this, ilevel);
                deltaAction =
                        (action == 0) ?
                                0 : action->getActionDifference(*this, ilevel);
                double piRatioCoA = -deltaAction + oldDeltaAction;
                double accRatioA2C = exp(lnTranProb + piRatioCoA);

                //Make the trial move to the rejected state C2B, and get the acc ratio and transition prob. C2B
                factor = defaultFactor;
                lnTranProb = mover.makeDelayedMove(*this, ilevel);
                double transC2B = mover.getForwardProb();
                double accRatioC2B = exp(lnTranProb - piRatioCoA + piRatioBoA);
                accRatioC2B = (accRatioC2B >= 1) ? 1 : accRatioC2B;
                double accRatioA2B2C = accRatioA2C * exp(transC2B + transA2B)
                        * (1 - accRatioC2B) / (1 - accRatioA2B);

                deltaAction = deltaAction - transC2B - transA2B
                        - log((1 - accRatioC2B) / (1 - accRatioA2B));
                if (RandomNumGenerator::getRand() > accRatioA2B2C)
                    return false;
            }
        } else {
            if (RandomNumGenerator::getRand() > acceptProb)
                return false;
        }
        oldDeltaAction = deltaAction;

        if (accRejEst)
            accRejEst->moveAccepted(ilevel);
    }

    // Move accepted.
    action->acceptLastMove();
    // Put moved beads in section beads.
    for (int islice = 0; islice < sectionBeads->getNSlice(); ++islice) {
        movingBeads->copySlice(identityIndex, islice, *sectionBeads,
                *movingIndex, islice);

    }
    // Append the current permutation to section permutation.
    const Permutation& perm(permutationChooser->getPermutation());
    Permutation temp(perm);
    for (int i = 0; i < nmoving; ++i) {
        temp[i] = (*sectionPermutation)[(*particleChooser)[i]];
    }
    for (int i = 0; i < nmoving; ++i) {
        (*sectionPermutation)[(*particleChooser)[i]] = temp[perm[i]];
    }
    return true;
}

void MultiLevelSampler::setAction(Action* act, const int level) {
    action = act;
}

AccRejEstimator*
MultiLevelSampler::getAccRejEstimator(const std::string& name) {
    std::ostringstream longName;
    longName << name << ": level " << nlevel << ", moving " << nmoving << " "
            << particleChooser->getName();
    return accRejEst = new AccRejEstimator(longName.str(), nlevel + 1);
}

int MultiLevelSampler::getFirstSliceIndex() const {
    return sectionChooser.getFirstSliceIndex();
}
