#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include "SpinMover.h"
#include "advancer/MultiLevelSampler.h"
#include "base/Beads.h"
#include "base/SimulationInfo.h"
#include "util/RandomNumGenerator.h"
#include <blitz/tinyvec.h>
#include <cstdlib>
#include <cmath>

SpinMover::SpinMover(const double lam, const int npart, const double tau)
  : freeMover(lam,npart,tau), lambda(npart), tau(tau) {
  lambda=lam; 
}

SpinMover::SpinMover(const SimulationInfo& simInfo, const int maxlevel,
  const double pgDelta)
  : freeMover(simInfo, maxlevel, pgDelta),
    lambda(simInfo.getNPart()), tau(simInfo.getTau()) {
  for (int i=0; i<simInfo.getNPart(); ++i) {
    const Species* species=&simInfo.getPartSpecies(i);
    lambda(i)=0.5/species->mass;
  }
}

SpinMover::~SpinMover() {
}

double SpinMover::makeMove(MultiLevelSampler& sampler, const int level) {
  const Beads<4> &sectionBeads =
    dynamic_cast<Beads<4>&>(*sampler.getSectionBeads().getAuxBeads(1));
  Beads<4>& movingBeads =
    dynamic_cast<Beads<4>&>(*sampler.getMovingBeads().getAuxBeads(1));
  const int nStride = 1 << level;
  const int nSlice=sectionBeads.getNSlice();
  const blitz::Array<int,1>& index=sampler.getMovingIndex(); 
  const int nMoving=index.size();
  blitz::Array<SVec,1> gaussRand(nMoving);
  double toldOverTnew=0;
  for (int islice=nStride; islice<nSlice-nStride; islice+=2*nStride) {
    RandomNumGenerator::makeGaussRand(gaussRand);
    for (int iMoving=0; iMoving<nMoving; ++iMoving) {
      const int i=index(iMoving);
      double sigma = sqrt(lambda(i)*tau*nStride);
      double inv2Sigma2 = 0.5/(sigma*sigma);
      // Calculate the new position.
      SVec midpoint=movingBeads.delta(iMoving,islice+nStride,-2*nStride)*0.5;
      midpoint+=movingBeads(iMoving,islice-nStride);
      SVec delta = gaussRand(iMoving); delta*=sigma;
      (movingBeads(iMoving,islice)=midpoint)+=delta;
      // Add transition probability for move.
      for (int idim=0;idim<4;++idim) {
        toldOverTnew += delta[idim]*delta[idim]*inv2Sigma2;
      }
      // Calculate and add reverse transition probability.
      midpoint=sectionBeads.delta(i,islice+nStride,-2*nStride)*0.5;
      midpoint+=sectionBeads(i,islice-nStride);
      delta=sectionBeads(i,islice); delta-=midpoint;
      for (int idim=0;idim<4;++idim) {
        toldOverTnew -= delta[idim]*delta[idim]*inv2Sigma2;
      }
    }
  }
  toldOverTnew=exp(toldOverTnew);
  return toldOverTnew;// * freeMover.makeMove(sampler,level);
}
