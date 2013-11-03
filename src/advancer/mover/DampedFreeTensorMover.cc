#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include "advancer/mover/DampedFreeTensorMover.h"
#include "advancer/MultiLevelSampler.h"
#include "base/Beads.h"
#include "base/SimulationInfo.h"
#include "util/RandomNumGenerator.h"
#include "util/SuperCell.h"
#include <cstdlib>
#include <blitz/tinyvec.h>
#include <cmath>
#include <iostream>

DampedFreeTensorMover::DampedFreeTensorMover(const SimulationInfo& simInfo,
        const int saturationLevel) :
        lambda(simInfo.getNPart()), tau(simInfo.getTau()), saturationLevel(
                saturationLevel) {
    for (int i = 0; i < simInfo.getNPart(); ++i) {
        lambda(i) = 0.5;
        lambda(i) /= (*simInfo.getPartSpecies(i).anMass);
    }
}

double DampedFreeTensorMover::makeMove(MultiLevelSampler& sampler,
        const int level) {
    const Beads<NDIM>& sectionBeads = sampler.getSectionBeads();
    Beads<NDIM>& movingBeads = sampler.getMovingBeads();
    const SuperCell& cell = sampler.getSuperCell();
    const int nStride = 1 << level;
    //std::cout << "sl:" << saturationLevel << "," << level << std::endl;
    const double lambdaScale =
            (saturationLevel >= level) ?
                    1.0 : pow(2.0, saturationLevel - level);
    const int nSlice = sectionBeads.getNSlice();
    const blitz::Array<int, 1>& index = sampler.getMovingIndex();
    const int nMoving = index.size();
    blitz::Array<Vec, 1> gaussRand(nMoving);
    double toldOverTnew = 0;
    for (int islice = nStride; islice < nSlice - nStride;
            islice += 2 * nStride) {
        RandomNumGenerator::makeGaussRand(gaussRand);
        for (int iMoving = 0; iMoving < nMoving; ++iMoving) {
            const int i = index(iMoving);
            Vec& lambdai = lambda(i);
            double sigmax = sqrt(lambdai[0] * lambdaScale * tau * nStride);
            double sigmay = sqrt(lambdai[1] * lambdaScale * tau * nStride);
            double sigmaz = sqrt(lambdai[2] * lambdaScale * tau * nStride);
            double inv2Sigma2x = 0.5 / (sigmax * sigmax);
            double inv2Sigma2y = 0.5 / (sigmay * sigmay);
            double inv2Sigma2z = 0.5 / (sigmaz * sigmaz);
            // Calculate the new position.
            Vec midpoint = movingBeads(iMoving, islice + nStride);
            cell.pbc(midpoint -= movingBeads(iMoving, islice - nStride)) *= 0.5;
            midpoint += movingBeads(iMoving, islice - nStride);
            cell.pbc(midpoint);
            Vec delta = gaussRand(iMoving);
            delta[0] *= sigmax; // cell.pbc(delta[0]);
            delta[1] *= sigmay; // cell.pbc(delta[1]);
            delta[2] *= sigmaz;
            cell.pbc(delta);
            (movingBeads(iMoving, islice) = midpoint) += delta;
            cell.pbc(movingBeads(iMoving, islice));
            // Add transition probability for move.
            toldOverTnew += delta[0] * delta[0] * inv2Sigma2x
                    + delta[1] * delta[1] * inv2Sigma2y
                    + delta[2] * delta[2] * inv2Sigma2z;
            // Calculate and add reverse transition probability.
            midpoint = sectionBeads(i, islice + nStride);
            cell.pbc(midpoint -= sectionBeads(i, islice - nStride)) *= 0.5;
            midpoint += sectionBeads(i, islice - nStride);
            cell.pbc(midpoint);
            delta = sectionBeads(i, islice);
            delta -= midpoint;
            cell.pbc(delta);
            toldOverTnew -= delta[0] * delta[0] * inv2Sigma2x
                    + delta[1] * delta[1] * inv2Sigma2y
                    + delta[2] * delta[2] * inv2Sigma2z;
        }
    }
    toldOverTnew = exp(toldOverTnew);
    return toldOverTnew;
}
