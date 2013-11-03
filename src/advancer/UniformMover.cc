#include "config.h"
#ifdef ENABLE_MPI
#include <mpi.h>
#endif
#include "UniformMover.h"
#include "DisplaceMoveSampler.h"
#include "DoubleDisplaceMoveSampler.h"
#include "base/Beads.h"
#include "base/SimulationInfo.h"
#include "stats/MPIManager.h"
#include "util/RandomNumGenerator.h"
#include "util/SuperCell.h"
#include <cmath>

#include <sstream>
#include <string>
UniformMover::UniformMover(const Vec dist, const MPIManager* mpi) :
        mpi(mpi), dist(dist) {
    std::cout << "Uniform mover with dist=" << dist << std::endl;
}

UniformMover::~UniformMover() {
}

double UniformMover::makeMove(VArray& displacement,
        const IArray& movingIndex) const {
    int npart = displacement.size();
    const Vec half = 0.5;
    RandomNumGenerator::makeRand(displacement);
    for (int i = 0; i < npart; i++) {
        for (int idim = 0; idim < NDIM; ++idim) {
            displacement(i)[idim] -= half[idim];
            displacement(i)[idim] *= dist[idim];
        }
    }
#ifdef ENABLE_MPI
    if (mpi && (mpi->getNWorker())>1) {
        mpi->getWorkerComm().Bcast(displacement.data(),npart*NDIM,MPI::DOUBLE,0);
    }
#endif
    return 0;
}

/*double UniformMover::makeMove(DoubleDisplaceMoveSampler& sampler) const {
 // typedef blitz::TinyVector<double,NDIM> Vec;
 // Beads<NDIM>& movingBeads=sampler.getMovingBeads();
 Beads<NDIM>& movingBeads1=sampler.getMovingBeads(1);
 Beads<NDIM>& movingBeads2=sampler.getMovingBeads(2);
 const SuperCell& cell=sampler.getSuperCell();
 const int nSlice = sampler.getNSlice();

 const IArray& index=sampler.getMovingIndex();
 const int nMoving=index.size();
 Array uniRand(nMoving*NDIM);// fix::makerand should take a vec. also.
 RandomNumGenerator::makeRand(uniRand);
 #ifdef ENABLE_MPI
 if (mpi && (mpi->getNWorker())>1) {
 mpi->getWorkerComm().Bcast(&uniRand(0),nMoving*NDIM,MPI::DOUBLE,0);
 }
 #endif

 double * dr = new double[NDIM];
 for (int i=0; i<NDIM; i++){
 for (int iMoving=0; iMoving<nMoving; ++iMoving) {
 dr[i] =  dist*(uniRand(NDIM*iMoving+i)-0.5);
 for (int islice=0; islice<nSlice; islice++) {
 //movingBeads(iMoving,islice)[i] += dr[i];
 movingBeads1(iMoving,islice)[i] += dr[i];
 cell.pbc(movingBeads1(iMoving,islice));
 
 movingBeads2(iMoving,islice)[i] += dr[i];
 cell.pbc(movingBeads2(iMoving,islice));
 }
 }
 }
 
 delete [] dr;
 return 0;
 }
 */
