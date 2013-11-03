#include "config.h"
#ifdef ENABLE_MPI
#include <mpi.h>
#endif
#include "VirialEnergyEstimator.h"
#include "action/Action.h"
#include "action/DoubleAction.h"
#include "base/SimulationInfo.h"
#include "stats/MPIManager.h"
#include "util/SuperCell.h"
#include <cstdlib>
#include <blitz/tinyvec.h>
#include <blitz/tinyvec-et.h>

VirialEnergyEstimator::VirialEnergyEstimator(
  const SimulationInfo& simInfo, const Action* action,
  const DoubleAction* doubleAction, const int nwindow, MPIManager *mpi,
  const std::string& unitName, double scale, double shift)
  : ScalarEstimator("virial_energy","scalar-energy/virial-energy",
                    unitName,scale,shift), 
    tau(simInfo.getTau()), energy(0), etot(0), enorm(0), 
    npart(simInfo.getNPart()), action(action), doubleAction(doubleAction),
    r0(npart), rav(npart), fav(npart), cell(*simInfo.getSuperCell()),
    nwindow(nwindow), isStatic(npart), mpi(mpi) {
  for (int i=0; i<npart; ++i) isStatic(i)=simInfo.getPartSpecies(i).isStatic;
}

void VirialEnergyEstimator::initCalc(const int nslice, const int firstSlice) {
  this->nslice=nslice;
  this->firstSlice=firstSlice;
  energy=0; rav=0.0; r0=0.0; fav=0.0;
}

void VirialEnergyEstimator::handleLink(const Vec& start, const Vec& end,
    const int ipart, const int islice, const Paths& paths) {
  if ((islice-firstSlice)%nwindow==0 && !isStatic(ipart)) {
    energy+=(1/(2*tau))*dot(fav(ipart),r0(ipart));
    r0(ipart)=0;
    rav(ipart)=0;
    fav(ipart)=0;
  } 
  int thisWindow=((islice-firstSlice)/nwindow<(nslice-1)/nwindow)
                ? nwindow : (nslice-1)%nwindow+1;
  Vec r=end-start; cell.pbc(r);
  rav(ipart)+=r;
  r0(ipart)+=(thisWindow-(islice-firstSlice)%nwindow)*r/thisWindow;
  double u(0),utau(0),ulambda(0); Vec fm=0.0, fp=0.0;
  if (action) action->getBeadAction(paths,ipart,islice,u,utau,ulambda,fm,fp);
  fav(ipart) += fm + fp;
  energy+=utau;
  if (!isStatic(ipart)) {
    energy += -(1.-1./thisWindow)*(NDIM/(2*tau))
              -(1/(2*tau))*dot((fm+fp),rav(ipart));
  }
  if (doubleAction) {
    u=0;utau=0;ulambda=0;fm=0.; fp=0.;
    doubleAction->getBeadAction(paths,ipart,islice,u,utau,ulambda,fm,fp,false);
    fav(ipart) += fm+fp;
    energy+=utau;
    if (!isStatic(ipart)) energy+=-(1/(2*tau))*dot((fm+fp),rav(ipart));
  }
}

void VirialEnergyEstimator::endCalc(const int lnslice) {
  for (int i=0; i<npart; ++i) {
    if (!isStatic(i)) energy+=(1/(2*tau))*dot(fav(i),r0(i));
  }
  int nslice=lnslice;
  #ifdef ENABLE_MPI
  if (mpi) {
    double buffer; int ibuffer;
    mpi->getWorkerComm().Reduce(&energy,&buffer,1,MPI::DOUBLE,MPI::SUM,0);
    mpi->getWorkerComm().Reduce(&lnslice,&ibuffer,1,MPI::INT,MPI::SUM,0);
    energy=buffer; nslice=ibuffer;
  }
  #endif
  energy/=nslice;
  etot+=energy; enorm+=1;
}
