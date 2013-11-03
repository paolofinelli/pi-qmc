#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include "GaussianDotAction.h"
#include "advancer/SectionSamplerInterface.h"
#include "base/Beads.h"
#include "base/Paths.h"
#include "base/SimulationInfo.h"
#include "util/SuperCell.h"

GaussianDotAction::GaussianDotAction(const double v0, const double alpha,
    const Vec &center, const SimulationInfo& simInfo) 
  : v0(v0), alpha(alpha), tau(simInfo.getTau()), center(center)  {
  std::cout << "Gaussian action, alpha=" << alpha
            << ", v0= " << v0 << ", center=" << center << std::endl;
}

double GaussianDotAction::getActionDifference(const SectionSamplerInterface& sampler,
                                         const int level) {
  const Beads<NDIM>& sectionBeads=sampler.getSectionBeads();
  const Beads<NDIM>& movingBeads=sampler.getMovingBeads();
  const SuperCell& cell=sampler.getSuperCell();
  const int nStride = 1 << level;
  const int nSlice=sectionBeads.getNSlice();
  const IArray& index=sampler.getMovingIndex(); 
  const int nMoving=index.size();
  double deltaAction=0;
  for (int islice=nStride; islice<nSlice-nStride; islice+=nStride) {
    for (int iMoving=0; iMoving<nMoving; ++iMoving) {
      // Add action for moving beads.
      Vec delta=movingBeads(iMoving,islice)-center;
      cell.pbc(delta);
      deltaAction+=v0*exp(-alpha*dot(delta,delta))*tau*nStride;
      // Subtract action for old beads.
      delta=sectionBeads(index(iMoving),islice)-center;
      cell.pbc(delta);
      deltaAction-=v0*exp(-alpha*dot(delta,delta))*tau*nStride;
    }
  }
  return deltaAction;
}

double GaussianDotAction::getTotalAction(const Paths& paths, int level) const {
  return 0;
}

void GaussianDotAction::getBeadAction(const Paths& paths, int ipart, int islice,
         double& u, double& utau, double& ulambda, Vec& fm, Vec& fp) const {
  u=utau=ulambda=0; fm=0.; fp=0.;
  Vec delta=paths(ipart,islice)-center;
  paths.getSuperCell().pbc(delta);
  double v=v0*exp(-alpha*dot(delta,delta));
  utau+=v;
  //fm=delta; fm*=v*denom*denom;
  //fp=delta; fp*=v*denom*denom;
  u=utau*tau;
}
