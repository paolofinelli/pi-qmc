#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include "PrimTorusAction.h"
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
This is a potential for a 2d ring. 
This is a modified potential based on PrimSHOAction
a=0.5*m*omega*omega and b is the radius from the centre 
of the ring out to the centre of the harmonic oscillator
Virial ---- works

Changelog 
-Removed loop over NDIMS
-defined x,y,z and r,z
-added ring potential and ring virial forces
-added Species option from SHOAction

Peter G McDonald 2009, Heriot Watt University, Scotland. pgm4@hw.ac.uk
*/

PrimTorusAction::PrimTorusAction(const double a, const double b, 
const double c,  const SimulationInfo &simInfo, int ndim, const Species &species) 
  : tau(simInfo.getTau()), a(a), b(b), c(c), ndim(ndim),
  ifirst(species.ifirst), npart(species.count){
}

double PrimTorusAction::getActionDifference(const SectionSamplerInterface& sampler,
                                         const int level) {
  const Beads<NDIM>& sectionBeads=sampler.getSectionBeads();
  const Beads<NDIM>& movingBeads=sampler.getMovingBeads();
  const SuperCell& cell=sampler.getSuperCell();
  const int nStride = 1 << level;
  const int nSlice=sectionBeads.getNSlice();
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
#if NDIM>2
	z=delta[2];
#endif
        r=sqrt(x*x + y*y);

      deltaAction+=(a*pow((r-b),2) + c*z*z)*ktstride;
      // Subtract action for old beads.
      delta=sectionBeads(i,islice);
      cell.pbc(delta);
      x=delta[0];
      y=delta[1];
#if NDIM>2
      z=delta[2];
#endif
      r=sqrt(x*x + y*y);
      deltaAction-=(a*pow((r-b),2) + c*z*z)*ktstride;
    }
  }
  return deltaAction;
}

double PrimTorusAction::getTotalAction(const Paths& paths, 
    const int level) const {
  return 0;
}

void PrimTorusAction::getBeadAction(const Paths& paths, int ipart, int islice,
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
#if NDIM>2
	z=delta[2];
#endif
	r=sqrt(x*x + y*y);
  utau=a*pow((r-b),2) + c*z*z;
  u=utau*tau;
  fm=((a/r)*(b-r)*(x+y) - c*z)*tau;
  fp=((a/r)*(b-r)*(x+y) - c*z)*tau;
}
