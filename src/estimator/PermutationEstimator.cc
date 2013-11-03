#include "config.h"
#ifdef ENABLE_MPI
#include <mpi.h>
#endif
#include "PermutationEstimator.h"
#include "base/SimulationInfo.h"
#include "base/Species.h"
#include "base/Paths.h"
#include "stats/MPIManager.h"
#include "util/Permutation.h"
#include <cstdlib>
#include <blitz/tinyvec.h>
#include <blitz/tinyvec-et.h>
#include <fstream>


PermutationEstimator::PermutationEstimator(const SimulationInfo& simInfo, const std::string& name,
		       const Species &s1, MPIManager *mpi)
  : BlitzArrayBlkdEst<1>(name, "histogram/permutation", IVecN(s1.count), true), 
    ifirst(s1.ifirst), nipart(s1.count), npart(simInfo.getNPart()),
    mpi(mpi) {
  value = 0.;
  norm = 0;
  visited = new bool[nipart];
  for (int i=0; i<nipart; ++i)   visited[i] = false;
}

PermutationEstimator::~PermutationEstimator() {
  delete [] visited;
}

void PermutationEstimator::evaluate(const Paths &paths) {
    const Permutation &perm(paths.getGlobalPermutation());

    for (int k=0; k<nipart; k++){
        int i = ifirst + k;
        if (!visited[k]){
            int cnt = 0;
            visited[k] = true;
            int j = i;
            while(perm[j] !=i){
                cnt++;
                j = perm[j];
                visited[j-ifirst]= true;
            }
            value(cnt) += 1.;
        }
    }
    for (int i=0; i<nipart; ++i)   visited[i] = false;
    norm += 1.;

}



void PermutationEstimator::averageOverClones(const MPIManager* mpi) {
#ifdef ENABLE_MPI
  if (mpi && mpi->isCloneMain()) {
    int rank = mpi->getCloneComm().Get_rank();
    int size = mpi->getCloneComm().Get_size();
    if (size>1) {
      reset();
      if (rank==0) {
#if MPI_VERSION==2
        mpi->getCloneComm().Reduce(MPI::IN_PLACE,&norm,1,MPI::DOUBLE,
                                   MPI::SUM,0);
        mpi->getCloneComm().Reduce(MPI::IN_PLACE,value.data(),value.size(),
                                   MPI::FLOAT,MPI::SUM,0);
#else
        double nbuff;
        ArrayN vbuff(n);
        mpi->getCloneComm().Reduce(&norm,&nbuff,1,MPI::DOUBLE,
                                   MPI::SUM,0);
        mpi->getCloneComm().Reduce(value.data(),vbuff.data(),value.size(),
                                   MPI::FLOAT,MPI::SUM,0);
        norm=nbuff;
        value=vbuff;
#endif
      } else {
        mpi->getCloneComm().Reduce(&norm,NULL,1,MPI::DOUBLE,MPI::SUM,0);
        mpi->getCloneComm().Reduce(value.data(),NULL,value.size(),
                                   MPI::FLOAT,MPI::SUM,0);
      }
    }
  }
#endif
  // Next add value to accumvalue and accumvalue2.
  accumvalue += value/norm;
  if (hasErrorFlag) accumvalue2 += (value*value)/(norm*norm);
  accumnorm+=1.; 
  value=0.; norm=0;
  ++iblock; 
}
