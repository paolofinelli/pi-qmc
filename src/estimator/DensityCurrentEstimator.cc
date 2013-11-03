#include "config.h"
#ifdef ENABLE_MPI
#include <mpi.h>
#endif
#include "DensityCurrentEstimator.h"
#include "base/LinkSummable.h"
#include "base/Paths.h"
#include "base/SimulationInfo.h"
#include "base/Species.h"
#include "stats/MPIManager.h"
#include "util/Distance.h"
#include "util/SuperCell.h"
#include <cstdlib>
#include <blitz/array.h>

DensityCurrentEstimator::DensityCurrentEstimator(const SimulationInfo& simInfo,
    const std::string& name, const Vec &min, const Vec &max, const IVec &nbin, 
    const IVecN &nbinN, const int &njbin,
    const DistArray &dist, int nstride, MPIManager *mpi) 
  : BlitzArrayBlkdEst<NDIM+2>(name,"dynamic-array/density-current",
                               nbinN,false),
    nsliceEff(simInfo.getNSlice()/nstride), nfreq(nbinN[NDIM+1]), 
    nstride(nstride), min(min), deltaInv(nbin/(max-min)), nbin(nbin), 
    tempj(njbin,simInfo.getNSlice()/nstride), tau(simInfo.getTau()),
    ax(simInfo.getSuperCell()->a[0]/2.), ntot(product(nbin)), dist(dist),  
    npart(simInfo.getNPart()), q(npart), njbin(njbin), dxinv(njbin/(ax*2)),
    mpi(mpi) {
  blitz::TinyVector<int,NDIM+1> tempDim;
  for (int i=0; i<NDIM; ++i) tempDim[i]=nbin[i];
  tempDim[NDIM]=nsliceEff;
  tempn.resize(tempDim);
  // Set up new views of the arrays for convenience.
  tempn_ = new DensityCurrentEstimator::CArray2(tempn.data(),
    blitz::shape(ntot,nsliceEff), blitz::neverDeleteData); 
  value_ = new DensityCurrentEstimator::FArray3(value.data(),
    blitz::shape(ntot,njbin,nfreq), blitz::neverDeleteData); 
  // Set up the FFT.
  fftw_complex *ptr = (fftw_complex*)tempn.data();
  fwdn = fftw_plan_many_dft(1,&nsliceEff,ntot,
                            ptr,0,1,nsliceEff,
                            ptr,0,1,nsliceEff,
                            FFTW_FORWARD,FFTW_MEASURE);
  ptr = (fftw_complex*)tempj.data();
  fwdj = fftw_plan_many_dft(1,&nsliceEff,njbin,
                            ptr,0,1,nsliceEff,
                            ptr,0,1,nsliceEff,
                            FFTW_FORWARD,FFTW_MEASURE);
  for (int i=0; i<npart; ++i) q(i)=simInfo.getPartSpecies(i).charge; 
}

DensityCurrentEstimator::~DensityCurrentEstimator() {
  for (int i=0; i<NDIM; ++i) delete dist[i];
  delete tempn_;
  delete value_;
  fftw_destroy_plan(fwdn);
  fftw_destroy_plan(fwdj);
}

void DensityCurrentEstimator::initCalc(const int nslice,
    const int firstSlice) {
  tempn=0.0;
  tempj=0.0;
}


void DensityCurrentEstimator::handleLink(const Vec& start, const Vec& end,
    const int ipart, const int islice, const Paths &paths) {
  // Calculate density distribution.
  int isliceBin = (islice/nstride+nsliceEff)%nsliceEff;
  Vec r=start;
  blitz::TinyVector<int,NDIM+1> ibin=0;
  ibin[NDIM]=isliceBin;
  for (int i=0; i<NDIM; ++i) {
    double d=(*dist[i])(r);
    ibin[i]=int(floor((d-min[i])*deltaInv[i]));
    if (ibin[i]<0 || ibin[i]>=nbin[i]) break;
    if (i==NDIM-1) tempn(ibin) += 1.0;
  }
  // Calculate current at x = 0, assuming no link is longer than a[0]/2.
  int ijbin=((int)((end[0]+ax)*dxinv+njbin))%njbin;
  int jjbin=((int)((start[0]+ax)*dxinv+njbin))%njbin;
  if (ijbin!=jjbin) {
    int nstep = ((ijbin-jjbin+3*njbin/2)%njbin)-njbin/2;
    int idir = (nstep>0)?1:-1;
    if (idir>0)
      for (int i=1;i<=nstep;++i)
	tempj((jjbin+i)%njbin,isliceBin)+=idir*q(ipart);
    else
      for (int i=1;i<=-nstep;++i)
	tempj((ijbin+i)%njbin,isliceBin)+=idir*q(ipart);
//  if (fabs(start[0]-end[0]) < ax && start[0]*end[0] < 0) {
//    if (start[0] > end[0]) tempj(0,isliceBin) -= q(ipart);
//    else tempj(0,isliceBin) += q(ipart);
  }
}


void DensityCurrentEstimator::endCalc(const int nslice) {
  // First move all data to 1st worker. 
  int workerID=(mpi)?mpi->getWorkerID():0;
  ///Need code for multiple workers!
#ifdef ENABLE_MPI
//    if (mpi) {
//      if (workerID==0) {
//        mpi->getWorkerComm().Reduce(&temp(0,0,0),&temp(0,0,0),
//                                    product(nbin),MPI::DOUBLE,MPI::SUM,0);
//      } else {
//        mpi->getWorkerComm().Reduce(MPI::IN_PLACE,&temp(0,0,0),
//                                    product(nbin),MPI::DOUBLE,MPI::SUM,0);
//      }
//    }
#endif
  if (workerID==0) {
    // Calculate autocorrelation function using FFT for convolution.
    fftw_execute(fwdn);
    fftw_execute(fwdj);
    double scale= 1./tau*nsliceEff;
    for (int i=0; i<ntot; ++i) 
      for (int j=0; j<njbin; ++j) 
	for (int ifreq=0; ifreq<nfreq; ++ifreq) 
	  (*value_)(i,j,ifreq) += scale 
	    * imag((*tempn_)(i,ifreq)*conj(tempj(j,ifreq)));
    norm+=1;
  }
}

