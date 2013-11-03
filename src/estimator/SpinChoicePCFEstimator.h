#ifndef __SpinChoicePCFEstimator_h_
#define __SpinChoicePCFEstimator_h_
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#ifdef ENABLE_MPI
#include <mpi.h>
#endif
#include "action/Action.h"
#include "action/ActionChoice.h"
#include "base/LinkSummable.h"
#include "base/Paths.h"
#include "base/ModelState.h"
#include "base/SimulationInfo.h"
#include "base/Species.h"
#include "base/SpinModelState.h"
#include "stats/BlitzArrayBlkdEst.h"
#include "stats/MPIManager.h"
#include "util/SuperCell.h"
#include "util/PairDistance.h"
#include <cstdlib>
#include <blitz/array.h>
#include <blitz/tinyvec-et.h>
#include <vector>
/** Pair correlation class for SpinChoiceFixedNodeAction. */
template <int N>
class SpinChoicePCFEstimator : public BlitzArrayBlkdEst<N>, public LinkSummable {
public:
  typedef blitz::Array<float,N> ArrayN;
  typedef blitz::Array<int,1> IArray;
  typedef blitz::TinyVector<double,NDIM> Vec;
  typedef blitz::TinyVector<double,N> VecN;
  typedef blitz::TinyVector<int,N> IVecN;
  typedef std::vector<PairDistance*> DistN;

  SpinChoicePCFEstimator(const SimulationInfo& simInfo, const std::string& name,
                  const Species &species, ModelState& modelState,
                  const VecN &min, const VecN &max, const IVecN &nbin, 
		  const DistN &dist, int ispin, bool samespin, MPIManager *mpi) 
    : BlitzArrayBlkdEst<N>(name,"array/spin-pair-correlation",nbin,true), 
      min(min), deltaInv(nbin/(max-min)), nbin(nbin), dist(dist),
      cell(*simInfo.getSuperCell()), temp(nbin), ispin(ispin), samespin(samespin),
      spinState((dynamic_cast<SpinModelState*>(&modelState))->getSpinState()),
      mpi(mpi){

		BlitzArrayBlkdEst<N>::max = new VecN(max);
		BlitzArrayBlkdEst<N>::min = new VecN(min);
  
    ifirst = species.ifirst;
    npart = ifirst + species.count; 

    BlitzArrayBlkdEst<N>::norm=0;
#ifdef ENABLE_MPI
    if (mpi) mpiBuffer.resize(nbin);
#endif
    if (ispin == 0)
      std::cout<<"SpinChoicePCFEstimator: the reference spin is up."<<std::endl;
    else
      std::cout<<"SpinChoicePCFEstimator: the reference spin is down."<<std::endl;
    if (samespin) 
      std::cout<<"SpinChoicePCFEstimator for same spin correlation."<<std::endl;
    else
      std::cout<<"SpinChoicePCFEstimator for opposite spin correlation."<<std::endl;
  }

  virtual ~SpinChoicePCFEstimator() {
    for (int i=0; i<N; ++i) delete dist[i];
  }
  /// Initialize the calculation.
  virtual void initCalc(const int nslice, const int firstSlice) {
    temp=0;
  }
  /// Add contribution from a link.
  virtual void handleLink(const Vec& start, const Vec& end,
         const int ipart, const int islice, const Paths &paths) {
    if (spinState(ipart) != ispin) return;
    if (ipart>=ifirst && ipart<npart) {
      int jpart = ifirst;
      while (jpart < npart) {
	if (samespin)
	  while (spinState(jpart) != ispin && jpart < npart) ++jpart;
	else 
	  while (spinState(jpart) == ispin && jpart < npart) ++jpart;
	if (jpart >= npart) return;
	if (ipart!=jpart) {
	  Vec r1=end; Vec r2=paths(jpart,islice);
	  IVecN ibin=0;
	  for (int i=0; i<N; ++i) {
	    double d=(*dist[i])(r1,r2,cell);
	    ibin[i]=int((d-min[i])*deltaInv[i]);
	    if (d<min[i] || ibin[i]>nbin[i]-1) break;
	    if (i==N-1) ++temp(ibin);
	  }
	}
	++jpart;
      }
    }
  }
  /// Finalize the calculation.
  virtual void endCalc(const int lnslice) {
    int nslice=lnslice;
    // First move all data to 1st worker. 
    int workerID=(mpi)?mpi->getWorkerID():0;
#ifdef ENABLE_MPI
    if (mpi) {
      int ibuffer;
      mpi->getWorkerComm().Reduce(temp.data(),mpiBuffer.data(),
                                  product(nbin),MPI::FLOAT,MPI::SUM,0);
      mpi->getWorkerComm().Reduce(&lnslice,&ibuffer,1,MPI::INT,MPI::SUM,0);
      temp = mpiBuffer;
      nslice = ibuffer;
    }
#endif
    temp /= nslice;
    if (workerID==0) {
      BlitzArrayBlkdEst<N>::value+=temp;
      BlitzArrayBlkdEst<N>::norm+=1;
    }
  }
  /// Clear value of estimator.
  virtual void reset() {}
  /// Evaluate for Paths configuration.
  virtual void evaluate(const Paths& paths) {paths.sumOverLinks(*this);}
private:
  VecN min;
  VecN deltaInv;
  IVecN nbin;
  DistN dist;
  const SuperCell& cell;
  ArrayN temp;
  int ifirst, npart;
  /// Reference spin.
  const int ispin;
  const bool samespin;
  const IArray &spinState;
  MPIManager *mpi;
#ifdef ENABLE_MPI
  ArrayN mpiBuffer;
#endif
};
#endif
