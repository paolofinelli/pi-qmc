#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include "PrimColloidalAction.h"
#include "advancer/SectionSamplerInterface.h"
#include "base/Beads.h"
#include "base/Paths.h"
#include "base/SimulationInfo.h"
#include "base/Species.h"
#include "util/SuperCell.h"
#include <cstdlib>
#include <blitz/tinyvec.h>
#include <blitz/tinyvec-et.h>

/*
This is a potential for a 3d Spherical Colloidal QD, with step like potential.
This is a modified potential based on PrimSHOAction

-added Species option from SHOAction

Peter G McDonald 2011, Heriot Watt University, Scotland. pgm4@hw.ac.uk
*/

PrimColloidalAction::PrimColloidalAction(const double B1, const double B2, 
const double V_lig, const double V_cdte, const double V_cdse, const SimulationInfo &simInfo, int ndim, const Species &species) 
  : tau(simInfo.getTau()), B1(B1), B2(B2),V_lig(V_lig),V_cdte(V_cdte),V_cdse(V_cdse), ndim(ndim),
  ifirst(species.ifirst), npart(species.count){
  std::cout << "PrimColloidalAction with B1=" << B1 << "B2= " << B2
            << " V_lig= " << V_lig << "V_cdte= " << V_cdte << "V_cdse= " << V_cdse << std::endl;
}

double PrimColloidalAction::getActionDifference(const SectionSamplerInterface& sampler,
                                         const int level) {
  const Beads<NDIM>& sectionBeads=sampler.getSectionBeads();
  const Beads<NDIM>& movingBeads=sampler.getMovingBeads();
  const SuperCell& cell=sampler.getSuperCell();
  const int nStride = 1 << level;
  const int nSlice = sectionBeads.getNSlice();
  const IArray& index=sampler.getMovingIndex(); 
  const int nMoving=index.size();
  double deltaAction=0;
  double ktstride = tau*nStride;
  for (int islice=nStride; islice<nSlice-nStride; islice+=nStride) {
    for (int iMoving=0; iMoving<nMoving; ++iMoving) {
      const int i=index(iMoving);
if (i<ifirst || i>=ifirst+npart) continue;
      // Add action for moving beads.
      Vec delta=movingBeads(iMoving,islice);
      cell.pbc(delta);
	double x=0;
	double y=0;
	double z=0;      
	double r=0;
	x=delta[0];
	y=delta[1];
	z=delta[2];
    r=sqrt(x*x + y*y + z*z);

if (sqrt(r*r)>0 && sqrt(r*r)<=B1) {
      deltaAction+=V_cdte*ktstride;
} else if (sqrt(r*r)>B1 && sqrt(r*r)<=B2) {
      deltaAction+=V_cdse*ktstride;
} else if (sqrt(r*r)>B2) {
      deltaAction+=V_lig*ktstride;
}


      // Subtract action for old beads.
      delta=sectionBeads(i,islice);
      cell.pbc(delta);
x=delta[0];
y=delta[1];
z=delta[2];
    r=sqrt(x*x + y*y + z*z);

if (sqrt(r*r)>0 && sqrt(r*r)<=B1) {
      deltaAction-=V_cdte*ktstride; }
else if	(sqrt(r*r)>B1 && sqrt(r*r)<=B2) {
      deltaAction-=V_cdse*ktstride;
} else if (sqrt(r*r)>B2) {
      deltaAction-=V_lig*ktstride;
}


    }
  }
  return deltaAction;
}

double PrimColloidalAction::getTotalAction(const Paths& paths, 
    const int level) const {
  return 0;
}

void PrimColloidalAction::getBeadAction(const Paths& paths, int ipart, int islice,
     double& u, double& utau, double& ulambda, Vec &fm, Vec &fp) const {
  Vec delta=paths(ipart,islice);
fm=0; fp=0; ulambda=0;
	double x=0;
	double y=0;
	double z=0;
	double r=0; 
   if (ipart<ifirst || ipart>=ifirst+npart) return;
	x=delta[0];
	y=delta[1];
	z=delta[2];
    r=sqrt(x*x + y*y + z*z);

if (sqrt(r*r)>0 && sqrt(r*r)<=B1) {
      utau=V_cdte; 
}
else if (sqrt(r*r)>B1 && sqrt(r*r)<=B2) {
      utau=V_cdse;
} else if (sqrt(r*r)>B2) {
      utau=V_lig;
}
  u=utau*tau;
}
