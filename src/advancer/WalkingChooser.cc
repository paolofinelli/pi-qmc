#include "config.h"
#ifdef ENABLE_MPI
#include <mpi.h>
#endif
#include "WalkingChooser.h"
#include "MultiLevelSampler.h"
#include "base/Beads.h"
#include "base/SimulationInfo.h"
#include "base/Species.h"
#include "util/PeriodicGaussian.h"
#include "util/Permutation.h"
#include "util/SuperCell.h"
#include "util/RandomNumGenerator.h"
#include <blitz/tinyvec-et.h>

WalkingChooser::WalkingChooser(const int nsize, const Species &species,
        const int nlevel, const SimulationInfo& simInfo) :
        PermutationChooser(nsize), SpeciesParticleChooser(species, nsize), t(
                npart, npart), cump(npart, npart), pg(NDIM), nsize(nsize), mass(
                species.mass), tau(simInfo.getTau()) {
    double alpha = mass / (2 * tau * (1 << nlevel));
    for (int idim = 0; idim < NDIM; ++idim) {
        double length = (*simInfo.getSuperCell())[idim];
        pg(idim) = new PeriodicGaussian(alpha, length);
    }
    // Initialize permutation to an n-cycle.
    for (int i = 0; i < nsize; ++i)
        (*permutation)[i] = (i + 1) % nsize;
}

WalkingChooser::~WalkingChooser() {
    for (PGArray::iterator i = pg.begin(); i != pg.end(); ++i)
        delete *i;
}

void WalkingChooser::chooseParticles() {
}

bool WalkingChooser::choosePermutation() {
    double tranProb = 0, revTranProb = 0;
    for (int ipart = 0; ipart < nsize; ++ipart) {
        if (ipart == 0) {
            // Select first particle at random.
            index(ipart) = (int) (npart * RandomNumGenerator::getRand());
            if (index(ipart) == npart)
                index(ipart) = npart - 1;
            //std :: cout <<"\n"<<"Begin : "<<index(0)<<"---->";
        } else {
            // Select subsequent particle from t(k1,k2)/h(k1)
            double h = cump(index(ipart - 1), npart - 1);
            double x = h * RandomNumGenerator::getRand();
            index(ipart) = iSearch(index(ipart - 1), x);

            /*test JS and this method
             double xx=x;
             for (int jpart=0; jpart<npart-1; ++jpart) {
             xx-=t(index(ipart-1),jpart);
             if (xx<0) {
             if (index(ipart)==jpart) std :: cout <<"FOUND Agreement"<<index(ipart)<<" is equal"<<jpart<<std:: endl;
             if (index(ipart)!=jpart) std :: cout <<"DIS DIS Agreement"<<index(ipart)<<" is NOT equal"<<jpart<<std:: endl;
             break;}
             }
             */

            // Reject if repeated (not a nsize permutiaton cycle).
            for (int jpart = 0; jpart < ipart; ++jpart) {
                if (index(ipart) == index(jpart))
                    return false;
            }
            // Accumulate inverse probabilities.
            revTranProb += h / t(index(ipart - 1), index(ipart - 1));
            tranProb += h / t(index(ipart - 1), index(ipart));
            //std :: cout <<index(ipart)<<"---->";
        }
    }
    // Accumulate the inverse probability to connect the cycle.
    double h = cump(index(nsize - 1), npart - 1);
    revTranProb += h / t(index(nsize - 1), index(nsize - 1));
    tranProb += h / t(index(nsize - 1), index(0));
    double accept = revTranProb / tranProb;
    //std :: cout <<index(nsize-1)<<"END"<<std :: endl<<std :: endl;

    prob = 1;
    for (int i = 0; i < nsize; ++i)
        prob *= t(index(i), index(i)) / t(index(i), index((i + 1) % nsize));
    // Add ifirst to particle IDs to convert to absolute IDs.
    index += ifirst;
    return RandomNumGenerator::getRand() < accept;
}

void WalkingChooser::init() {
    // Setup the table of free particle propagotor values.
    const Beads<NDIM> &sectionBeads = multiLevelSampler->getSectionBeads();
    const SuperCell &cell = multiLevelSampler->getSuperCell();
    const int nslice = sectionBeads.getNSlice();

    for (int ipart = 0; ipart < npart; ++ipart) {
        for (int jpart = 0; jpart < npart; ++jpart) {
            Vec delta = sectionBeads(ipart + ifirst, 0);
            delta -= sectionBeads(jpart + ifirst, nslice - 1);
            cell.pbc(delta);
            t(ipart, jpart) = 1;
            for (int idim = 0; idim < NDIM; ++idim) {
                t(ipart, jpart) *= pg(idim)->evaluate(delta[idim]);
            }
        }
    }

    for (int ipart = 0; ipart < npart; ++ipart) {
        cump(ipart, 0) = t(ipart, 0);
    }

    for (int ipart = 0; ipart < npart; ++ipart) {
        for (int jpart = 1; jpart < npart; ++jpart) {
            cump(ipart, jpart) = cump(ipart, jpart - 1) + t(ipart, jpart);
        }
    }
}

int WalkingChooser::iSearch(int part, double x) {

    if (x < cump(part, 0))
        return 0;

    if (cump(part, npart - 2) < x)
        return npart - 1;

    int ilo = 0;
    int ihi = npart - 2;
    int mid = 0;
    for (int k = 0; k < npart - 1; k++) {
        if (ihi == ilo + 1)
            return ihi;
        mid = (ilo + ihi) / 2;
        if (cump(part, mid) > x) {
            ihi = mid;
        } else {
            ilo = mid;
        }
    }
    return mid;
}

void WalkingChooser::setMLSampler(const MultiLevelSampler *mls) {
    multiLevelSampler = mls;
}
