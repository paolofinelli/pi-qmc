#ifndef __PairCFEstimator_h_
#define __PairCFEstimator_h_
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#ifdef ENABLE_MPI
#include <mpi.h>
#endif
#include "base/LinkSummable.h"
#include "base/Paths.h"
#include "base/SimulationInfo.h"
#include "base/Species.h"
#include "stats/BlitzArrayBlkdEst.h"
#include "stats/MPIManager.h"
#include "util/SuperCell.h"
#include "util/PairDistance.h"
#include <cstdlib>
#include <blitz/array.h>
#include <blitz/tinyvec-et.h>
#include <vector>
/** Pair correlation class.
 *  Handles general pair correlation functions for many different geometries.
 *  @version $Revision$
 *  @author John Shumway  */
template <int N>
class PairCFEstimator : public BlitzArrayBlkdEst<N>, public LinkSummable {
public:
  typedef blitz::Array<float,N> ArrayN;
  typedef blitz::Array<int,1> IArray;
  typedef blitz::TinyVector<double,NDIM> Vec;
  typedef blitz::TinyVector<double,N> VecN;
  typedef blitz::TinyVector<int,N> IVecN;
  typedef std::vector<PairDistance*> DistN;
  /// Constructor.
  PairCFEstimator(const SimulationInfo& simInfo, const std::string& name,
                  const Species *speciesList, const int nspecies, const VecN &min, 
                  const VecN &max, const IVecN &nbin, const DistN &dist,
                  MPIManager *mpi) 
    : BlitzArrayBlkdEst<N>(name,"array/pair-correlation",nbin,true), 
    min(min), deltaInv(nbin/(max-min)), nbin(nbin), dist(dist), nspecies(nspecies), 
      cell(*simInfo.getSuperCell()), temp(nbin), mpi(mpi) {

		BlitzArrayBlkdEst<N>::max = new VecN(max);
		BlitzArrayBlkdEst<N>::min = new VecN(min);
  
    ifirst = speciesList[0].ifirst;
    nipart = speciesList[0].count; 

    jfirst.resize(nspecies);
    njpart.resize(nspecies);
    for (int ispec=1; ispec < nspecies; ispec++){
      jfirst(ispec) = speciesList[ispec].ifirst;
      njpart(ispec) = speciesList[ispec].count; 
    }



    BlitzArrayBlkdEst<N>::norm=0;
#ifdef ENABLE_MPI
    if (mpi) mpiBuffer.resize(nbin);
#endif
  }
  /// Virtual destructor.
  virtual ~PairCFEstimator() {
    for (int i=0; i<N; ++i) delete dist[i];
  }
  /// Initialize the calculation.
  virtual void initCalc(const int nslice, const int firstSlice) {
    temp=0;
  }
  /// Add contribution from a link.
  virtual void handleLink(const Vec& start, const Vec& end,
         const int ipart, const int islice, const Paths &paths) {
    if (ipart>=ifirst && ipart<ifirst+nipart) {
      for (int ispec=1; ispec<nspecies; ispec++){
	for (int jpart=jfirst(ispec); jpart<(jfirst(ispec)+njpart(ispec)); ++jpart) {
	 
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

	}
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
  const int nspecies;
  SuperCell cell;
  ArrayN temp;
  int ifirst, nipart;
  IArray jfirst,njpart;
  MPIManager *mpi;
#ifdef ENABLE_MPI
  ArrayN mpiBuffer;
#endif
};
#endif
