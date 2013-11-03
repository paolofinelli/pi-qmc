#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include "SHOAction.h"
#include "advancer/SectionSamplerInterface.h"
#include "base/Beads.h"
#include "base/Paths.h"
#include "base/Species.h"
#include "util/SuperCell.h"

SHOAction::SHOAction(const double tau, const double omega, const double mass,
                     const int ndim, const Species &species, const Vec& center) 
  : tau(tau), omega(omega), mass(mass), ndim(ndim),
    ifirst(species.ifirst), npart(species.count), center(center) {
  std::cout << "SHOAction with omega=" << omega 
            << " centered at " << center << std::endl;
}

double SHOAction::getActionDifference(const SectionSamplerInterface& sampler,
                                      const int level) {
  const Beads<NDIM>& sectionBeads=sampler.getSectionBeads();
  const Beads<NDIM>& movingBeads=sampler.getMovingBeads();
  const int nStride = 1 << level;
  const int nSlice=sectionBeads.getNSlice();
  const IArray& index=sampler.getMovingIndex(); 
  const int nMoving=index.size();
  double deltaAction=0;
  double wt=omega*tau*nStride;
  double sinhwt=sinh(wt);
  double coshwt=cosh(wt);
  double m=mass;
  double div1=1.0/(2.0*sinhwt);
  double div2=1.0/(2.0*tau*nStride);
  for (int islice=nStride; islice<nSlice; islice+=nStride) {
    for (int iMoving=0; iMoving<nMoving; ++iMoving) {
      const int i=index(iMoving);
      if (i<ifirst || i>=ifirst+npart) continue;
      // Add action for moving beads.
      Vec r1=movingBeads(iMoving,islice)-center; 
      Vec r0=movingBeads(iMoving,islice-nStride)-center; 
      Vec delta=r1-r0;
      double r0r0=0, r0r1=0, r1r1=0, delta2=0;
      for (int idim=(NDIM-ndim); idim<NDIM; ++idim) {
        r0r0+=r0[idim]*r0[idim];
        r0r1+=r0[idim]*r1[idim];
        r1r1+=r1[idim]*r1[idim];
        delta2+=delta[idim]*delta[idim];
      }
      deltaAction+=m*omega*((r1r1+r0r0)*coshwt-2.0*r0r1)*div1-m*delta2*div2;
      // Subtract action for old beads.
      r1=sectionBeads(i,islice)-center;
      r0=sectionBeads(i,islice-nStride)-center;
      delta=r1-r0;
      r0r0=0; r0r1=0; r1r1=0; delta2=0;
      for (int idim=(NDIM-ndim); idim<NDIM; ++idim) {
        r0r0+=r0[idim]*r0[idim];
        r0r1+=r0[idim]*r1[idim];
        r1r1+=r1[idim]*r1[idim];
        delta2+=delta[idim]*delta[idim];
      }
      deltaAction-=m*omega*((r1r1+r0r0)*coshwt-2.0*r0r1)*div1-m*delta2*div2;
    }
  }
  return deltaAction;
}

double SHOAction::getTotalAction(const Paths& paths, const int level) const {
  return 0;
}

void SHOAction::getBeadAction(const Paths& paths, int ipart, int islice,
    double& u, double& utau, double& ulambda, Vec &fm, Vec &fp) const {
  u=utau=0; fm=0; fp=0;
  if (ipart<ifirst || ipart>=ifirst+npart) return;
  double wt=omega*tau;
  double sinhwt=sinh(wt);
  double coshwt=cosh(wt);
  double cschwt=1.0/sinhwt;
  double cothwt=coshwt*cschwt;
  double tin=1.0/tau;
  double m=mass;
  Vec r1=paths(ipart,islice)-center;
  Vec r0=paths(ipart,islice,-1)-center;
  Vec r2=paths(ipart,islice,1)-center;
  Vec delta = paths.delta(ipart,islice,-1);
  double r0r0=0, r0r1=0, r1r1=0, delta2=0;
  for (int idim=(NDIM-ndim); idim<NDIM; ++idim) {
    r0r0+=r0[idim]*r0[idim];
    r0r1+=r0[idim]*r1[idim];
    r1r1+=r1[idim]*r1[idim];
    delta2+=delta[idim]*delta[idim];
  }
  fm-= (m*omega*(r1*coshwt-r0)*cschwt-m*delta*tin);
  utau = 0.5*ndim*omega*cothwt
        +0.5*m*omega*omega
          *( (r1r1+r0r0)*(1-cothwt*cothwt)
             +2.0*r0r1*coshwt*cschwt*cschwt)
        +0.5*m*delta2*tin*tin-0.5*ndim*tin;
  u=0.5*ndim*log(2*3.14159265358979*sinhwt/(m*omega))+m*omega*((r1r1+r0r0)
	  *coshwt-2.0*r0r1)*cschwt*0.5 -0.5*log(2.0*3.14159265358979*tau/m)
	  -0.5*m*delta2/tau;
  delta=paths.delta(ipart,islice,1);
  fp-= (m*omega*(r1*coshwt-r2)*cschwt-m*delta*tin);
  for (int idim=0; idim<(NDIM-ndim); ++idim) {fm[idim]=0; fp[idim]=0;}
}

double SHOAction::getActionDifference(const Paths &paths, 
       const VArray &displacement, int nmoving, const IArray &movingIndex,
       int iFirstSlice, int iLastSlice) {
  double deltaAction=0;
  double wt=omega*tau;
  double sinhwt=sinh(wt);
  double coshwt=cosh(wt);
  double m=mass;
  double div1=1.0/(2.0*sinhwt);
  for (int i=0; i<nmoving; ++i) {
    int ipart = movingIndex(i);
    if (ipart<ifirst || ipart>=ifirst+npart) break;
    for (int islice=iFirstSlice; islice<=iLastSlice; ++islice) {
      Vec r1=paths(ipart,islice)-center;
      Vec r0=paths(ipart,islice,-1)-center;
      Vec r2=paths(ipart,islice,1)-center;
      double r0r0=0, r0r1=0, r1r1=0;
      for (int idim=(NDIM-ndim); idim<NDIM; ++idim) {
        r0r0+=r0[idim]*r0[idim];
        r0r1+=r0[idim]*r1[idim];
        r1r1+=r1[idim]*r1[idim];
      }
      deltaAction -= m*omega*((r1r1+r0r0)*coshwt-2.0*r0r1)*div1;
      r1 += displacement(i);
      r0 += displacement(i);
      r2 += displacement(i);
      r0r0=0; r0r1=0; r1r1=0;
      for (int idim=(NDIM-ndim); idim<NDIM; ++idim) {
        r0r0+=r0[idim]*r0[idim];
        r0r1+=r0[idim]*r1[idim];
        r1r1+=r1[idim]*r1[idim];
      }
      deltaAction += m*omega*((r1r1+r0r0)*coshwt-2.0*r0r1)*div1;
    }
  }
  return deltaAction;
}
