#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include "RingGateAction.h"
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
This is a potential for a 2d ring mimicking the gate potential in experiments.
The potential is GVolt * (tanh(s(theta+theta0)) - tanh(s(theta-theta0))).
GVolt is the potential of the top gate in energy unit.
s is a parameter.
theta0 is defined w.r.t x-axis and cannot be zero.

Virial -- not works

@author Jianheng Liu
*/

RingGateAction::RingGateAction(const SimulationInfo &simInfo, 
  const double GVolt, const double s, const double theta0, 
  const Species &species)
  : tau(simInfo.getTau()), GVolt(GVolt), s(s), theta0(theta0), 
    normalConst(tanh(s * theta0) - tanh(s * (-theta0))),
    ifirst(species.ifirst), npart(species.count) {
}

double RingGateAction::getActionDifference(const SectionSamplerInterface& sampler, const int level) {
  const Beads<NDIM>& sectionBeads=sampler.getSectionBeads();
  const Beads<NDIM>& movingBeads=sampler.getMovingBeads();
  const SuperCell& cell=sampler.getSuperCell();
  const int nStride = 1 << level;
  const int nSlice=sectionBeads.getNSlice();
  const IArray& index=sampler.getMovingIndex(); 
  const int nMoving=index.size();
  double deltaAction=0;
  double ktstride = tau*nStride;
  double x = 0;
  double y = 0;
  double theta = 0;
  for (int islice=nStride; islice<nSlice-nStride; islice+=nStride) {
    for (int iMoving=0; iMoving<nMoving; ++iMoving) {
      const int i=index(iMoving);
      if (i<ifirst || i>=ifirst+npart) continue;
      // Add action for moving beads.
      Vec delta=movingBeads(iMoving,islice);
      cell.pbc(delta);
      x = delta[0];
      y = delta[1];
      theta = atan2(y,x);
      deltaAction+=GVolt * (tanh(s * (theta + theta0)) - tanh(s * (theta - theta0))) / normalConst * ktstride;
      // Subtract action for old beads.
      delta = sectionBeads(i,islice);
      cell.pbc(delta);
      x = delta[0];
      y = delta[1];
      theta = atan2(y,x);
      deltaAction-=GVolt * (tanh(s * (theta + theta0)) - tanh(s * (theta - theta0))) / normalConst * ktstride;
    }
  }
  return deltaAction;
}

double RingGateAction::getTotalAction(const Paths& paths, const int level) const {
  return 0;
}

void RingGateAction::getBeadAction(const Paths& paths, int ipart, int islice, double& u, double& utau, double& ulambda, Vec &fm, Vec &fp) const {
  Vec delta = paths(ipart,islice);
  fm = 0; fp = 0; ulambda = 0;
  if (ipart < ifirst || ipart >= ifirst + npart) return;
  double x = delta[0];
  double y = delta[1];
  double theta = 0;
  theta = atan2(y,x);
  utau=GVolt * (tanh(s * (theta + theta0)) - tanh(s * (theta - theta0))) / normalConst;
  u = utau * tau;
}
