#include "config.h"
#ifdef ENABLE_MPI
#include <mpi.h>
#endif
#include "FreeMoverPBC.h"
#include "advancer/MultiLevelSampler.h"
#include "base/Beads.h"
#include "base/SimulationInfo.h"
#include "util/PeriodicGaussian.h"
#include "util/RandomNumGenerator.h"
#include "util/SuperCell.h"
#include <cstdlib>
#include <blitz/tinyvec.h>
#include <cmath>

FreeMoverPBC::FreeMoverPBC(const double lam, const int npart, const double tau) :
        lambda(npart), tau(tau) {
    lambda = lam;
}

FreeMoverPBC::FreeMoverPBC(const SimulationInfo& simInfo, const int maxlevel,
        const double pgDelta) :
        lambda(simInfo.getNPart()), tau(simInfo.getTau()), pg(maxlevel + 2,
                simInfo.getNSpecies(), NDIM), length(simInfo.getSuperCell()->a), specIndex(
                simInfo.getNPart()) {
    for (int i = 0; i < simInfo.getNPart(); ++i) {
        const Species* species = &simInfo.getPartSpecies(i);
        lambda(i) = 0.5 / species->mass;
        for (int j = 0; j < simInfo.getNSpecies(); ++j) {
            if (species == &simInfo.getSpecies(j)) {
                specIndex(i) = j;
                break;
            }
        }
    }
    const int nspec = simInfo.getNSpecies();
    pg.resize(maxlevel + 2, nspec, NDIM);
    for (int ilevel = 0; ilevel <= maxlevel + 1; ++ilevel) {
        for (int ispec = 0; ispec < nspec; ++ispec) {
            double alpha = simInfo.getSpecies(ispec).mass
                    / (tau * (1  << ilevel));
            for (int idim = 0; idim < NDIM; ++idim) {
                if (PeriodicGaussian::numberOfTerms(alpha, length[idim]) < 16) {
                    pg(ilevel, ispec, idim) = new PeriodicGaussian(alpha,
                            length[idim]);
                } else {
                    pg(ilevel, ispec, idim) = 0;
                }
            }
        }
    }
}

FreeMoverPBC::~FreeMoverPBC() {
    for (PGArray::iterator i = pg.begin(); i != pg.end(); ++i)
        delete *i;
}

double FreeMoverPBC::makeMove(MultiLevelSampler& sampler, const int level) {
    const Beads<NDIM>& sectionBeads = sampler.getSectionBeads();
    Beads<NDIM>& movingBeads = sampler.getMovingBeads();
    const SuperCell& cell = sampler.getSuperCell();
    const int nStride = 1 << level;
    const int nSlice = sectionBeads.getNSlice();
    double factor = sampler.getFactor();
    const blitz::Array<int, 1>& index = sampler.getMovingIndex();
    const int nMoving = index.size();
    blitz::Array<Vec, 1> gaussRand(nMoving);
    gaussRand = 0.0;
    double toldOverTnew = 0.;
    for (int islice = nStride; islice < nSlice - nStride;
            islice += 2 * nStride) {
        RandomNumGenerator::makeGaussRand(gaussRand);
        for (int iMoving = 0; iMoving < nMoving; ++iMoving) {
            const int i = index(iMoving);
            const int ispec = specIndex(i);
            double sigma = factor * sqrt(lambda(i) * tau * nStride);
            double inv2Sigma2 = 0.5 / (sigma * sigma);
            // Calculate the new position.
            // First sample periodic midpoint, starting with closest image.
            Vec midpoint = movingBeads.delta(iMoving, islice + nStride,
                    -2 * nStride);
            cell.pbc(midpoint);
            midpoint *= 0.5;
            Vec midpoint2 = midpoint + 0.5 * cell.a;
            cell.pbc(midpoint2);
            for (int idim = 0; idim < NDIM; ++idim) {
                double weight1 = 1.0, weight2 = 1.0;
                if (pg(level, ispec, idim)) {
                    pg(level, ispec, idim)->evaluate(midpoint[idim]);
                    weight1 = pg(level, ispec, idim)->getValue();
                    pg(level, ispec, idim)->evaluate(midpoint2[idim]);
                    weight2 = pg(level, ispec, idim)->getValue();
                } else {
                    weight1 = exp(
                            -inv2Sigma2 * midpoint[idim] * midpoint[idim]);
                    weight2 = exp(
                            -inv2Sigma2 * midpoint2[idim] * midpoint2[idim]);
                }
                if (RandomNumGenerator::getRand()
                        > weight1 / (weight1 + weight2)) {
                    midpoint[idim] = midpoint2[idim];
                }
            }
            midpoint += movingBeads(iMoving, islice - nStride);

            // Now sample about midpoint
            movingBeads(iMoving, islice) = midpoint
                    + gaussRand(iMoving) * sigma;
            cell.pbc(movingBeads(iMoving, islice));

            // Calculate forward and reverse transition probability.
            Vec deltapnew = movingBeads.delta(iMoving, islice, -nStride);
            cell.pbc(deltapnew);
            Vec deltannew = movingBeads.delta(iMoving, islice, +nStride);
            cell.pbc(deltannew);
            Vec deltanew = movingBeads.delta(iMoving, islice + nStride,
                    -2 * nStride);
            cell.pbc(deltanew);

            Vec deltapold = sectionBeads.delta(i, islice, -nStride);
            cell.pbc(deltapold);
            Vec deltanold = sectionBeads.delta(i, islice, +nStride);
            cell.pbc(deltanold);
            Vec deltaold = sectionBeads.delta(i, islice + nStride,
                    -2 * nStride);
            cell.pbc(deltaold);

            double temp = 1.;
            for (int idim = 0; idim < NDIM; ++idim) {
                PeriodicGaussian* gaussian = pg(level + 1, ispec, idim);
                if (gaussian) {
                    temp *= gaussian->evaluate(deltapold[idim]);
                    temp *= gaussian->evaluate(deltanold[idim]);
                    temp /= gaussian->evaluate(deltapnew[idim]);
                    temp /= gaussian->evaluate(deltannew[idim]);
                } else {
                    toldOverTnew -= deltapold[idim] * deltapold[idim]
                            * inv2Sigma2 * 0.5;
                    toldOverTnew -= deltanold[idim] * deltanold[idim]
                            * inv2Sigma2 * 0.5;
                    toldOverTnew += deltapnew[idim] * deltapnew[idim]
                            * inv2Sigma2 * 0.5;
                    toldOverTnew += deltannew[idim] * deltannew[idim]
                            * inv2Sigma2 * 0.5;
                }
                gaussian = pg(level + 2, ispec, idim);
                if (gaussian) {
                    temp /= gaussian->evaluate(deltaold[idim]);
                    temp *= gaussian->evaluate(deltanew[idim]);
                } else {
                    toldOverTnew += deltaold[idim] * deltaold[idim] * inv2Sigma2
                            * 0.25;
                    toldOverTnew -= deltanew[idim] * deltanew[idim] * inv2Sigma2
                            * 0.25;
                }
            }
            toldOverTnew += log(temp);
        }
    }
    return toldOverTnew; //Return the log of the probability.
}

// Delayed Rejection 
double FreeMoverPBC::makeDelayedMove(MultiLevelSampler& sampler,
        const int level) {
    const Beads<NDIM>& rejectedBeads = sampler.getRejectedBeads();
    Beads<NDIM>& movingBeads = sampler.getMovingBeads();
    const SuperCell& cell = sampler.getSuperCell();
    const int nStride = 1 << level;
    const int nSlice = movingBeads.getNSlice();
    double factor = sampler.getFactor();
    const blitz::Array<int, 1>& index = sampler.getMovingIndex();
    const int nMoving = index.size();

    double toldOverTnew = 0;
    forwardProb = 0;
    for (int islice = nStride; islice < nSlice - nStride;
            islice += 2 * nStride) {
        for (int iMoving = 0; iMoving < nMoving; ++iMoving) {
            const int i = index(iMoving);
            const int ispec = specIndex(i);
            double sigma = factor * sqrt(lambda(i) * tau * nStride);
            double inv2Sigma2 = 0.5 / (sigma * sigma);
            double temp = 1.0;
            // Calculate forward transition probability C2B
            Vec midpoint = movingBeads.delta(iMoving, islice + nStride,
                    -2 * nStride);
            cell.pbc(midpoint) *= 0.5;
            midpoint += movingBeads(iMoving, islice - nStride);
            cell.pbc(midpoint);
            Vec delta = rejectedBeads(iMoving, islice);
            delta -= midpoint;
            cell.pbc(delta);
            for (int idim = 0; idim < NDIM; ++idim) {
                if (pg(level, ispec, idim)) {
                    temp *= pg(level, ispec, idim)->evaluate(delta[idim]);
                } else {
                    double tmp = delta[idim] * delta[idim] * inv2Sigma2;
                    forwardProb -= tmp;
                    toldOverTnew += tmp;
                }
            }

            forwardProb += log(temp);
            temp = 1.0 / temp;

            // Calculate inverse transition probability B2C i.e. T(R1->R2)=numerator
            midpoint = rejectedBeads.delta(iMoving, islice + nStride,
                    -2 * nStride);
            cell.pbc(midpoint) *= 0.5;
            midpoint += rejectedBeads(iMoving, islice - nStride);
            cell.pbc(midpoint);
            delta = movingBeads(iMoving, islice);
            delta -= midpoint;
            cell.pbc(delta);
            for (int idim = 0; idim < NDIM; ++idim) {
                if (pg(level, ispec, idim)) {
                    temp *= pg(level, ispec, idim)->evaluate(fabs(delta[idim]));
                } else {
                    toldOverTnew -= delta[idim] * delta[idim] * inv2Sigma2;
                }
            }
            toldOverTnew += log(temp);
        }
    }
    return toldOverTnew; //Return the log of the probability.
}

