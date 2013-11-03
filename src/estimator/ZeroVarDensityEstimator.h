#ifndef __ZeroVarDensityEstimator_h_
#define __ZeroVarDensityEstimator_h_
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#ifdef ENABLE_MPI
#include <mpi.h>
#endif
#include "action/DoubleAction.h"
#include "action/Action.h"
#include "base/Paths.h"
#include "base/SimulationInfo.h"
#include "base/Species.h"
#include "base/LinkSummable.h"
#include "base/Paths.h"
#include "stats/BlitzArrayBlkdEst.h"
#include "stats/MPIManager.h"
#include "util/SuperCell.h"
#include <cstdlib>
#include <blitz/array.h>
#include <blitz/tinyvec-et.h>
#include <vector>

class ZeroVarDensityEstimator : public BlitzArrayBlkdEst<1>, public LinkSummable {
public:
  typedef blitz::Array<int,1> IArray;
  typedef blitz::Array<float,1> Array;
  typedef blitz::TinyVector<double,NDIM> Vec;
  /// Constructor.
ZeroVarDensityEstimator(const SimulationInfo& simInfo, 
   const std::string& name, const Species *speciesList,
   const int nspecies, const double &min, const double &max, 
   const int &nbin, const Action * action, 
   const DoubleAction * doubleAction, MPIManager *mpi) 
  : BlitzArrayBlkdEst<1>(name,"array/zero-var-density",IVecN(nbin),false),
    lambda(simInfo.getNPart()),
    tau(simInfo.getTau()), dh((max-min)/nbin), nbin(nbin), nspecies(nspecies),
    cell(*simInfo.getSuperCell()), temp(nbin), action(action), 
    doubleAction(doubleAction),mpi(mpi) { 
  ifirst = speciesList[0].ifirst;
  nipart = speciesList[0].count; 
  double npartotal=nipart;

  zCharge=speciesList[0].charge;
  jfirst.resize(nspecies);
  njpart.resize(nspecies);
  for (int ispec=1; ispec < nspecies; ispec++){
    jfirst(ispec) = speciesList[ispec].ifirst;
    njpart(ispec) = speciesList[ispec].count; 
    npartotal+=  njpart(ispec);
  }
  double vol=1;
  for (int i=0;i < NDIM; i++) {
    vol *= cell[i];
  }
  rho = npartotal/vol;
  for (int i=0; i<simInfo.getNPart(); ++i) {
    const Species* species=&simInfo.getPartSpecies(i);
    lambda(i)=0.5/species->mass;
  }
  temp.resize(nbin);
  temp=0;
  BlitzArrayBlkdEst<1>::norm=0;
#ifdef ENABLE_MPI
  if (mpi) mpiBuffer.resize(nbin);
#endif
}

/// Virtual destructor.
virtual ~ZeroVarDensityEstimator() {
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
      for (int jpart=jfirst(ispec);
           jpart<(jfirst(ispec)+njpart(ispec)); ++jpart) {
        if (ipart!=jpart) {
          double u(0),utau(0),ulambda(0); 
          Vec fm=0.0, fp=0.0, gradU=0.0;
          if (action) {
            action->getBeadAction(paths,jpart,islice,u,utau,ulambda,fm,fp);
          }
          gradU=fp+fm;
	  if (doubleAction) {
	    u=0;utau=0;ulambda=0;fm=0.; fp=0.;
	    doubleAction->getBeadAction(paths,jpart,islice,
                                        u,utau,ulambda,fm,fp,false);
	    gradU+=fp+fm;
          }
         Vec rij =end-paths(jpart,islice);
         rij=cell.pbc(rij);
         double mag_rij = sqrt(dot(rij,rij));
         Vec gradInvrij =  (rij)/(mag_rij*mag_rij*mag_rij);
         for (int ibin = 0; ibin < nbin; ibin++){
           double u = ibin*dh;
           if (mag_rij >=u) temp(ibin) += dot(gradU, gradInvrij);
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
                                (nbin),MPI::FLOAT,MPI::SUM,0);
    mpi->getWorkerComm().Reduce(&lnslice,&ibuffer,1,MPI::INT,MPI::SUM,0);
    temp = mpiBuffer;
    nslice = ibuffer; 
  }
#endif
  temp /= (nslice*4*3.14159265358979);
  if (workerID==0) {
    BlitzArrayBlkdEst<1>::value+=temp;
    BlitzArrayBlkdEst<1>::norm+=1;
  }
  temp=0;
}

/// Clear value of estimator.
virtual void reset() {}

/// Evaluate for Paths configuration.
virtual void evaluate(const Paths& paths) {
  paths.sumOverLinks(*this);
}

private:
  double rho;
  //int npartotal;
  int ifirst, nipart;
  IArray jfirst,njpart;
  Array lambda;
  const double tau;
  double dh;
  int nbin;
  const int nspecies;
  SuperCell cell;
  Array temp;
  double zCharge;
  const Action * action;
  const DoubleAction * doubleAction;
  MPIManager *mpi;
#ifdef ENABLE_MPI
  Array mpiBuffer;
#endif
};
#endif
