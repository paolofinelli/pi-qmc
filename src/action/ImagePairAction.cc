#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include "ImagePairAction.h"
#include "advancer/SectionSamplerInterface.h"
#include "base/Beads.h"
#include "base/Paths.h"
#include "base/SimulationInfo.h"
#include "util/SuperCell.h"
#include <vector>
#include <fstream>
#include <blitz/tinyvec-et.h>

ImagePairAction::ImagePairAction(const Species& s1, const Species& s2,
            const std::string& filename, const SimulationInfo& simInfo, 
            const int norder, const IVec nimage, const bool isDMD, int exLevel) 
  : PairAction(s1, s2, filename, simInfo, norder, false, isDMD, exLevel), 
    nimage(nimage) { 
  std::cout << "Using images: " << nimage << std::endl;
}

ImagePairAction::ImagePairAction(const Species& s1, const Species& s2,
            const EmpiricalPairAction &action, 
            const SimulationInfo& simInfo, const int norder, const IVec nimage,
            const double rmin, const double rmax,
            const int ngpts, int exLevel) 
  : PairAction(s1,s2,action,simInfo,norder,rmin,rmax,ngpts,false,exLevel),
    nimage(nimage) {
  std::cout << "Using images: " << nimage << std::endl;
} 

double ImagePairAction::getActionDifference(const SectionSamplerInterface& sampler,
                                         const int level) {
  const Beads<NDIM>& sectionBeads=sampler.getSectionBeads();
  const Beads<NDIM>& movingBeads=sampler.getMovingBeads();
  const SuperCell& cell=sampler.getSuperCell();
  const int nStride = 1 << level;
  const int nSlice=sectionBeads.getNSlice();
  const IArray& index=sampler.getMovingIndex(); 
  const int nMoving=index.size();
  double deltaAction=0;
  for (int iMoving=0; iMoving<nMoving; ++iMoving) {
    const int i=index(iMoving);
    int jbegin,jend;
    if (i>=ifirst1 && i<ifirst1+npart1) {
      jbegin=ifirst2; jend=ifirst2+npart2;
    } else
    if (i>=ifirst2 && i<ifirst2+npart2) {
      jbegin=ifirst1; jend=ifirst1+npart1;
    } else 
    continue; //Particle not in this interaction.
    for (int j=jbegin; j<jend; ++j) {
      bool isMoving=false; int jMoving=0;
      for (int k=0;k<nMoving;++k) {
        if (j==index(k)) {isMoving=true; jMoving=k; break;}
      }
      if (isMoving && i<j) continue; //Don't double count moving interactions.
      Vec prevDelta =sectionBeads(i,0);
      prevDelta-=sectionBeads(j,0); cell.pbc(prevDelta);
      Vec prevMovingDelta=prevDelta;
      //for (int img=-0; img<0; ++img) {
      for (int img=-nimage[0]; img<=nimage[0]; ++img) {
        if (img==0 && i==j) continue; //Skip self-interaction.
        Vec imgVec=0.; imgVec[0]=cell.a[0]*img;
        double prevR=sqrt(dot(prevDelta+imgVec,prevDelta+imgVec));
        double prevMovingR=prevR;
      for (int islice=nStride; islice<nSlice; islice+=nStride) {
        // Add action for moving beads.
        Vec delta=movingBeads(iMoving,islice);
        delta-=(isMoving)?movingBeads(jMoving,islice):sectionBeads(j,islice);
        cell.pbc(delta);
        double r=sqrt(dot(delta+imgVec,delta+imgVec));
        double q=0.5*(r+prevMovingR);
  //std::cout<<" r=" << r << " q=" << q << std::endl;
       	if (level==0) {
          Vec svec=delta-prevMovingDelta; double s2=dot(svec,svec)/(q*q);
          deltaAction+=uk0(q,s2);
        } else { 
          deltaAction+=u00(q);
        }
  //std::cout<< " deltaAction1 = " << deltaAction << std::endl;
        prevMovingDelta=delta; prevMovingR=r;
        // Subtract action for old beads.
        delta=sectionBeads(i,islice);
        delta-=sectionBeads(j,islice);
        cell.pbc(delta);
        r=sqrt(dot(delta+imgVec,delta+imgVec));
        q=0.5*(r+prevR);
        if (level==0) {
          Vec svec=delta-prevDelta; double s2=dot(svec,svec)/(q*q);
          deltaAction-=uk0(q,s2);
        } else {
          deltaAction-=u00(q);
        }
        prevDelta=delta; prevR=r;
      }
      }
    }
  }
  //std::cout<< " deltaAction1 = " << deltaAction << std::endl;
  return deltaAction*=nStride;
}

double ImagePairAction::getTotalAction(const Paths& paths, int level) const {
  return 0;
}

void ImagePairAction::getBeadAction(const Paths& paths, int ipart, int islice,
    double& u, double& utau, double& ulambda, Vec& fm, Vec& fp) const {
  u=utau=ulambda=0; fm=0.; fp=0.;
  int jbegin,jend;
  if (ipart>=ifirst1 && ipart<ifirst1+npart1) {
    jbegin=ifirst2; jend=ifirst2+npart2;
  } else
  if (ipart>=ifirst2 && ipart<ifirst2+npart2) {
    jbegin=ifirst1; jend=ifirst1+npart1;
  } else 
  return; //Particle not in this interaction.
  SuperCell cell=paths.getSuperCell();
  for (int j=jbegin; j<jend; ++j) {
    Vec delta=paths(ipart,islice);
    delta-=paths(j,islice); cell.pbc(delta);
    Vec prevDelta=paths(ipart,islice,-1);
    prevDelta-=paths(j,islice,-1); cell.pbc(prevDelta);
    Vec nextDelta=paths(ipart,islice,+1);
    nextDelta-=paths(j,islice,+1); cell.pbc(nextDelta);
    for (int img=-nimage[0]; img<=nimage[0]; ++img) {
    //for (int img=0; img<=0; ++img) {
      if (img==0 && ipart==j) continue;
      Vec imgVec=0.; imgVec[0]=cell.a[0]*img;
      double r=sqrt(dot(delta+imgVec,delta+imgVec));
      double q=0.5*(r+sqrt(dot(prevDelta+imgVec,prevDelta+imgVec))); 
      Vec svec=delta-prevDelta; double s2=dot(svec,svec)/(q*q);
      double v,vtau,vq,vs2;
      uk0CalcDerivatives(q,s2,v,vtau,vq,vs2);
      u += 0.5*v;
      utau += 0.5*vtau;
      fm -= vq*(delta+imgVec)/(2*r) + vs2*(2*svec/(q*q) 
          - s2*(delta+imgVec)/(q*r));
      // And force contribution from next slice.
      q=0.5*(r+sqrt(dot(nextDelta+imgVec,nextDelta+imgVec)));
      svec=delta-nextDelta; s2=dot(svec,svec)/(q*q);
      uk0CalcDerivatives(q,s2,v,vtau,vq,vs2);
      fp -= vq*(delta+imgVec)/(2*r) + vs2*(2*svec/(q*q) 
          - s2*(delta+imgVec)/(q*r));
    }
  }
}
