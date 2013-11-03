#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include "DotGeomAction.h"
#include <cstdlib>
#include <blitz/tinyvec.h>
#include "advancer/SectionSamplerInterface.h"
#include "base/Beads.h"
#include "base/Paths.h"
#include "util/SuperCell.h"

DotGeomAction::DotGeomAction(const double tau)
  : tau(tau) {
}

double DotGeomAction::getActionDifference(const SectionSamplerInterface& sampler,
                                         const int level) {
  double deltaAction=0;
#if NDIM==3
  const Beads<NDIM>& sectionBeads=sampler.getSectionBeads();
  const Beads<NDIM>& movingBeads=sampler.getMovingBeads();
  const SuperCell& cell=sampler.getSuperCell();
  const int nStride = 1 << level;
  const int nSlice=sectionBeads.getNSlice();
  const int npart=sectionBeads.getNPart();
  const IArray& index=sampler.getMovingIndex(); 
  const int nMoving=index.size();
  const double thickness = 30.23563; // 1.6 nm
  const double height = 75.589075; // 4.0 nm
  const double diameter = 755.89075; // 40.0 nm
  const double egap = 0.0558222; // 1.519 eV
  const double ve = 0.04663487; // 1.269 eV
  const double vh = 0.004409917; // 0.120 eV
  const double d2over4h = 0.25*diameter*diameter/height;
  for (int islice=nStride; islice<nSlice-nStride; islice+=nStride) {
    for (int iMoving=0; iMoving<nMoving; ++iMoving) {
      const int i=index(iMoving);
      // Add action for moving beads.
      Vec r=movingBeads(iMoving,islice);
      cell.pbc(r);
      if (r[2]<0) {
        if (r[2]>-thickness) {
          deltaAction += tau * nStride * ((i<npart/2)?ve:-vh); 
        } else {
          deltaAction += tau * nStride * ((i<npart/2)?egap:0.); 
        }
      } else {
        if (r[0]*r[0]+r[1]*r[1] < (height-r[2])*(r[2]+d2over4h)) {
          deltaAction += tau * nStride * ((i<npart/2)?ve:-vh); 
        } else {
          deltaAction += tau * nStride * ((i<npart/2)?egap:0.); 
        }
      }
      // Subtract action for old beads.
      r=sectionBeads(i,islice);
      cell.pbc(r);
      if (r[2]<0) {
        if (r[2]>-thickness) {
          deltaAction -= tau * nStride * ((i<npart/2)?ve:-vh); 
        } else {
          deltaAction -= tau * nStride * ((i<npart/2)?egap:0.); 
        }
      } else {
        if (r[0]*r[0]+r[1]*r[1] < (height-r[2])*(r[2]+d2over4h)) {
          deltaAction -= tau * nStride * ((i<npart/2)?ve:-vh); 
        } else {
          deltaAction -= tau * nStride * ((i<npart/2)?egap:0.); 
        }
      }
    }
  }
#endif
  return deltaAction;
}

double DotGeomAction::getTotalAction(const Paths& paths, 
    const int level) const {
  return 0;
}

void DotGeomAction::getBeadAction(const Paths& paths, int ipart, int islice,
    double& u, double& utau, double& ulambda, Vec &fm, Vec &fp) const {
  Vec r=paths(ipart,islice);
  utau = 0;
#if NDIM==3
  const double thickness = 30.23563; // 1.6 nm
  const double height = 75.589075; // 4.0 nm
  const double diameter = 755.89075; // 40.0 nm
  const double egap = 0.0558222; // 1.519 eV
  const double ve = 0.04663487; // 1.269 eV
  const double vh = 0.004409917; // 0.120 eV
  const double d2over4h = 0.25*diameter*diameter/height;
  const int npart = paths.getNPart();
  if (r[2]<0) {
    if (r[2]>-thickness) {
      utau += (ipart<npart/2)?ve:-vh; 
    } else {
      utau += (ipart<npart/2)?egap:0.; 
    }
  } else {
    if (r[0]*r[0]+r[1]*r[1] < (height-r[2])*(r[2]+d2over4h)) {
      utau += (ipart<npart/2)?ve:-vh; 
    } else {
      utau += (ipart<npart/2)?egap:0.; 
    }
  }
#endif
  u=utau*tau;
}
