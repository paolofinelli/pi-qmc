#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include "SpringTensorAction.h"
#include "advancer/SectionSamplerInterface.h"
#include "base/Beads.h"
#include "base/Paths.h"
#include "base/SimulationInfo.h"
#include "base/Species.h"
#include "util/SuperCell.h"

SpringTensorAction::SpringTensorAction(const SimulationInfo& simInfo)
  : lambda(simInfo.getNPart()), tau(simInfo.getTau()) {
  for (int i=0; i<simInfo.getNPart(); ++i) {
    lambda(i)=0.5;
    lambda(i)/=(*simInfo.getPartSpecies(i).anMass);
  }
}

double SpringTensorAction::getActionDifference(
    const SectionSamplerInterface& sampler, const int level) {
  const Beads<NDIM>& sectionBeads=sampler.getSectionBeads();
  const Beads<NDIM>& movingBeads=sampler.getMovingBeads();
  const SuperCell& cell=sampler.getSuperCell();
  const int nStride = 1 << level;
  const int nSlice=sectionBeads.getNSlice();
  const IArray& index=sampler.getMovingIndex(); 
  const int nMoving=index.size();
  double deltaAction=0;
  for (int islice=nStride; islice<nSlice; islice+=nStride) {
    for (int iMoving=0; iMoving<nMoving; ++iMoving) {
      const int i=index(iMoving);
      double inv2Sigma2x = 0.25/(lambda(i)[0]*tau*nStride);
      double inv2Sigma2y = 0.25/(lambda(i)[1]*tau*nStride);
      double inv2Sigma2z = 0.25/(lambda(i)[2]*tau*nStride);
      // Add action for moving beads.
      Vec delta=movingBeads(iMoving,islice);
      delta-=movingBeads(iMoving,islice-nStride);
      cell.pbc(delta);
      deltaAction+=(delta[0]*delta[0]*inv2Sigma2x+
                    delta[1]*delta[1]*inv2Sigma2y+
                    delta[2]*delta[2]*inv2Sigma2z);
      // Subtract action for old beads.
      delta=sectionBeads(i,islice);
      delta-=sectionBeads(i,islice-nStride);
      cell.pbc(delta);
      deltaAction-=(delta[0]*delta[0]*inv2Sigma2x+
                    delta[1]*delta[1]*inv2Sigma2y+
                    delta[2]*delta[2]*inv2Sigma2z);
    }
  }
  return deltaAction;
}

double SpringTensorAction::getTotalAction(const Paths& paths, int level) const {
  return 0;
}

void SpringTensorAction::getBeadAction(const Paths& paths, const int ipart,
    const int islice, double &u, double &utau, double &ulambda, 
    Vec &fm, Vec &fp) const {
  u=utau=ulambda=0; fm=0.; fp=0.; 
  Vec delta = paths.delta(ipart,islice,-1);
  for (int i=0; i<NDIM; ++i) {
    utau += 0.5/tau - delta[i]*delta[i]/(4.0*lambda(ipart)[i]*tau*tau);
    fm[i] = -delta[i]/(2*lambda(ipart)[i]*tau);
  }
  delta = paths.delta(ipart,islice,1);
  for (int i=0; i<NDIM; ++i) {
    fp[i] = -delta[i]/(2*lambda(ipart)[i]*tau);
  }
}
