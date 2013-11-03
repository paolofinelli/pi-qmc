#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include "PairAction.h"
#include "interaction/PairIntegrator.h"
#include "advancer/DisplaceMoveSampler.h"
#include "advancer/SectionSamplerInterface.h"
#include "base/Beads.h"
#include "base/Paths.h"
#include "base/SimulationInfo.h"
#include "util/SuperCell.h"
#include <vector>
#include <fstream>
#include <blitz/tinyvec-et.h>
PairAction::PairAction(const Species& s1, const Species& s2,
            const std::string& filename, const SimulationInfo& simInfo, 
            const int norder, const bool hasZ, int exLevel) 
  : tau(simInfo.getTau()), species1(s1), species2(s2),
    ifirst1(s1.ifirst), ifirst2(s2.ifirst),
    npart1(s1.count), npart2(s2.count), norder(norder),
    hasZ(hasZ), exLevel(exLevel), mass(s1.mass) {
  std::cout << "constructing PairAction";
  std::cout << " : species1= " <<  s1 << "species2= " <<  s2;
  std::cout << " : squarer filename (LOG grid assumed)=" << filename << std::endl;
  std::vector<Array> buffer; buffer.reserve(300);
  std::ifstream dmfile((filename+".pidmu").c_str()); 
  std::string temp;
  double r;
  int ndata=1;// count diagonal term
  if (hasZ) {
    ndata += norder*(norder+3)/2;
  } else {
    ndata+=norder;
  }
  Array u(ndata);

  // read .pidmu file into u 
  // Correct the fact that actions tabulated with squarer are with respect to s and z
  // not s/q and z/q (dimensionless) like is done with pi.
  while (dmfile) {
    dmfile >> r;
    int iorder=0;
    for (int k=0; k<ndata; ++k) {
      dmfile >> u(k); 
      if (k > iorder*(iorder+3)/2) iorder++;
      u(k)*=pow(r,2*iorder);
    }
    if (dmfile) buffer.push_back(u.copy());
  }

  // copy u into ugrid
  ngpts=buffer.size();
  ugrid.resize(ngpts,2,ndata);
  for (int i=0; i<ngpts; ++i) {
    for (int k=0; k<ndata; ++k) ugrid(i,0,k)=buffer[i](k);
  }

  // now copy the action beta derivatives directly to ugrid. Assuming LOG grid from squarer.
  std::ifstream dmefile((filename+".pidme").c_str());
  for (int i=0; i<ngpts; ++i) {
    dmefile >> r; 
    if (i==0) {
      rgridinv = 1.0/r;
    }
    if (i==ngpts-1) {
      logrratioinv = (ngpts-1)/log(r*rgridinv);
    }
    int iorder=0;
    for (int k=0; k<ndata; ++k) {
      dmefile >> ugrid(i,1,k); 
      if (k > iorder*(iorder+3)/2) iorder++;
      ugrid(i,1,k)*=pow(r,2*iorder);
    }
  }
 
  // check the squarer
  std::ofstream file((filename+".pidmucheck").c_str());
  file.precision(8);
  file.setf(file.scientific,file.floatfield); 
  file.setf(file.showpos);
  for (int i=0; i<ngpts; ++i) {      
    double r=(1./rgridinv)*exp(i/logrratioinv);
    file <<r;   
    int iorder=0;
    for (int k=0; k<ndata; ++k) {  
      if (k > iorder*(iorder+3)/2) iorder++;
      file << "  " << ugrid(i,0,k)/pow(r,2*iorder);
    }
    file << std::endl;
  }
  write("",hasZ);
}



/////////// 
PairAction::PairAction(const Species& s1, const Species& s2,
            const std::string& filename, const SimulationInfo& simInfo, 
            const int norder, const bool hasZ, const bool isDMD, int exLevel) 
  : tau(simInfo.getTau()), species1(s1), species2(s2),
    ifirst1(s1.ifirst), ifirst2(s2.ifirst),
    npart1(s1.count), npart2(s2.count), norder(norder), isDMD(isDMD),
    hasZ(hasZ), exLevel(exLevel), mass(s1.mass) {
  //std::cout << "constructing PairAction" << std::endl;
  //std::cout << "species1= " <<  s1 << "species2= " <<  s2 << std::endl;
  //std::cout << "filename=" << filename << std::endl;
  std::vector<Array> buffer; buffer.reserve(300);
  std::ifstream dmfile((filename+".dmu").c_str()); std::string temp;
  double r;
  double junk;
  int ndata = (hasZ) ? (norder+1)*(norder+2)/2 : norder+1;
  Array u(ndata);
  dmfile >> r; if (isDMD) dmfile >> junk; 
  for (int k=0; k<ndata; ++k) {
    dmfile >> u(k);
    if (isDMD && k>0) u(k)/=r*r;
  }
  getline(dmfile,temp); buffer.push_back(u.copy());
  rgridinv=1./r;
  dmfile >> r; if (isDMD) dmfile >> junk;
  for (int k=0; k<ndata; ++k) {
    dmfile >> u(k); 
    if (isDMD && k>0) u(k)/=r*r;
  }
  getline(dmfile,temp); buffer.push_back(u.copy());
  logrratioinv = 1/log(r*rgridinv);
  while (dmfile) {
    dmfile >> r; if (isDMD) dmfile >> junk;
    for (int k=0; k<ndata; ++k) {
      dmfile >> u(k); 
      if (isDMD && k>0) u(k)/=r*r;
    }
    getline(dmfile,temp); if (dmfile) buffer.push_back(u.copy());
  }
  ngpts=buffer.size();
  ugrid.resize(ngpts,2,ndata);
  for (int i=0; i<ngpts; ++i) {
    for (int k=0; k<ndata; ++k) ugrid(i,0,k)=buffer[i](k);
  }
  std::ifstream dmefile((filename+".dme").c_str());
  for (int i=0; i<ngpts; ++i) {
    dmefile >> r; if (isDMD) dmefile >> junk;
    for (int k=0; k<ndata; ++k) {
      dmefile >> ugrid(i,1,k);
      if (isDMD && k>0) ugrid(i,1,k)/=r*r;
    }
    getline(dmefile,temp);
  }
  if (isDMD) { // DMD drops potential contribution to tau derivative.
    std::ifstream potfile((filename+".pot").c_str());
    for (int i=0; i<ngpts; ++i) {
      double v;
      potfile >> r >> v; getline(potfile,temp);
      ugrid(i,1,0)+=v;
    }
  }
  std::ofstream file((filename+".dat").c_str());
  for (int i=0;i<100; ++i) file << i*0.1 << " " << u00(i*0.1) << std::endl;
}

PairAction::PairAction(const Species& s1, const Species& s2,
            const EmpiricalPairAction &action, const SimulationInfo& simInfo, 
            const int norder, const double rmin, const double rmax,
            const int ngpts, const bool hasZ, int exLevel) 
  : tau(simInfo.getTau()), ngpts(ngpts), rgridinv(1.0/rmin),
    logrratioinv((ngpts-1)/log(rmax/rmin)), 
    ugrid(ngpts,2,(hasZ?(norder+1)*(norder+2)/2:norder+1)),
    species1(s1), species2(s2), ifirst1(s1.ifirst), ifirst2(s2.ifirst),
    npart1(s1.count), npart2(s2.count), norder(norder), hasZ(hasZ), 
    exLevel(exLevel), mass(s1.mass) {
  std::cout << "Constructing PairAction" << std::endl;
  std::cout << "Species1= " <<  s1 << "species2= " <<  s2 << std::endl;
  ugrid=0;
  for (int i=0; i<ngpts; ++i) {
    double r=(1./rgridinv)*exp(i/logrratioinv);
    int idata=0;
    for (int iorder=0; iorder<norder+1; ++iorder) {
      for (int ioff=0; ioff<=(hasZ?iorder:0); ++ioff) {
        ugrid(i,0,idata)=action.u(r,idata);
        ugrid(i,1,idata)=action.utau(r,idata);
        ++idata;
      }
    }
  }
}

PairAction::PairAction(const Species& s1, const Species& s2,
            PairIntegrator &integrator, const SimulationInfo& simInfo, 
            const int norder, const double rmin, const double rmax,
            const int ngpts, int exLevel) 
  : tau(simInfo.getTau()), ngpts(ngpts), rgridinv(1.0/rmin),
    logrratioinv((ngpts-1)/log(rmax/rmin)), 
    ugrid(ngpts,2,norder+1),
    species1(s1), species2(s2), ifirst1(s1.ifirst), ifirst2(s2.ifirst),
    npart1(s1.count), npart2(s2.count), norder(norder), hasZ(false),
    exLevel(exLevel), mass(s1.mass) {
std::cout << "constructing PairAction" << std::endl;
std::cout << "species1= " <<  s1 << "species2= " <<  s2 << std::endl;
  ugrid=0;
  int ndata = (norder+1)*(norder+2)/2;
  for (int i=0; i<ngpts; ++i) {
    double r=(1./rgridinv)*exp(i/logrratioinv);
    integrator.integrate(r);
    Array u = integrator.getU();
    for (int idata=0; idata<ndata; ++idata) ugrid(i,0,idata) = u(idata);
    integrator.integrate(r,1.01);
    u = integrator.getU();
    for (int idata=0; idata<ndata; ++idata) ugrid(i,1,idata) = u(idata);
    integrator.integrate(r,0.99);
    u = integrator.getU();
    for (int idata=0; idata<ndata; ++idata) {
      ugrid(i,1,idata) -= u(idata);
      ugrid(i,1,idata) /= 0.02*tau;
    }
  }
}


double PairAction::getActionDifference(const SectionSamplerInterface& sampler,
                                         const int level) {
  const Beads<NDIM>& sectionBeads=sampler.getSectionBeads();
  const Beads<NDIM>& movingBeads=sampler.getMovingBeads();
  const SuperCell& cell=sampler.getSuperCell();
  const int nStride = 1 << level;
  const double invTauEff = 1./(tau*nStride);
  const int nSlice=sectionBeads.getNSlice();
  const IArray& index=sampler.getMovingIndex(); 
  const int nMoving=index.size();
  double deltaAction=0;
  for (int iMoving=0; iMoving<nMoving; ++iMoving) {
    const int i=index(iMoving);
    int jbegin,jend;
    if (i>=ifirst1 && i<ifirst1+npart1) {
      jbegin=ifirst2; jend=ifirst2+npart2;
    } else {
      if (i>=ifirst2 && i<ifirst2+npart2) {
        jbegin=ifirst1; jend=ifirst1+npart1;
       } else  {
      continue; //Particle not in this interaction.
      } 
    }
    for (int j=jbegin; j<jend; ++j) {
      bool isMoving=false; int jMoving=0;
      for (int k=0;k<nMoving;++k) {
        if (j==index(k)) {isMoving=true; jMoving=k; break;}
      }
      if (isMoving && i<=j) continue; //Don't double count moving interactions.
      Vec prevDelta =sectionBeads(i,0)-sectionBeads(j,0);
      cell.pbc(prevDelta);
      Vec prevMovingDelta=prevDelta;
      double prevR=sqrt(dot(prevDelta,prevDelta));
      double prevMovingR=prevR;
      for (int islice=nStride; islice<nSlice; islice+=nStride) {
        // Add action for moving beads.
        double action=0.;
        Vec delta=movingBeads(iMoving,islice);
        delta-=(isMoving)?movingBeads(jMoving,islice):sectionBeads(j,islice);
        cell.pbc(delta);
        double r=sqrt(dot(delta,delta));
        double q=0.5*(r+prevMovingR);
       	if (level<3 && norder>0) {
          Vec svec=delta-prevMovingDelta; double s2=dot(svec,svec)/(q*q);
          if (hasZ) {
            double z=(r-prevMovingR)/q;
            action+=uk0(q,s2,z*z)*nStride;
          } else {
            action+=uk0(q,s2)*nStride;
          }
        } else { 
          action+=u00(q)*nStride;
        }
       	if (level<=exLevel) {//Include exchange effect if requested.
          Vec r1  = movingBeads(iMoving,islice);
          Vec r1p = movingBeads(iMoving,islice-nStride);
          Vec r2  = (isMoving) ?  movingBeads(jMoving,islice)
                               : sectionBeads(j,islice);
          Vec r2p = (isMoving) ?  movingBeads(jMoving,islice-nStride)
                               : sectionBeads(j,islice-nStride);
          Vec d1 = r1-r1p; Vec d2 = r2-r2p; Vec e1 = r1-r2p; Vec e2 = r2-r1p;
          cell.pbc(d1); cell.pbc(d2); cell.pbc(e1); cell.pbc(e2);
          double g0  = exp(-0.5*mass*(dot(d1,d1)+dot(d2,d2))*invTauEff);
          double g0X = exp(-0.5*mass*(dot(e1,e1)+dot(e2,e2))*invTauEff);
          if (g0X/g0 > 1e-100) {
            double actX=0.;
       	    if (level<3 && norder>0) {
              Vec svec=delta+prevMovingDelta; double s2=dot(svec,svec)/(q*q);
              if (hasZ) {
                double z=(r-prevMovingR)/q;
                actX+=uk0(q,s2,z*z)*nStride;
              } else {
                actX+=uk0(q,s2)*nStride;
              }
            } else { 
              actX+=u00(q)*nStride;
            }
            double gfull = g0*exp(-action)-g0X*exp(-actX);
            double g0full = g0-g0X;
            if (gfull>0 && g0full >0 && gfull<g0full) {
              action = -log(gfull/g0full);
            } else {
              action = 0;
            }
          }
        }
        deltaAction += action;
        prevMovingDelta=delta; prevMovingR=r;
        // Subtract action for old beads.
        action=0.;
        delta=sectionBeads(i,islice)-sectionBeads(j,islice);
        cell.pbc(delta);
        r=sqrt(dot(delta,delta));
        q=0.5*(r+prevR);
        if (level<3 && norder>0) {
          Vec svec=delta-prevDelta; double s2=dot(svec,svec)/(q*q);
          if (hasZ) {
            double z=(r-prevR)/q;
            action+=uk0(q,s2,z*z)*nStride;
          } else {
            action+=uk0(q,s2)*nStride;
          }
        } else {
          action+=u00(q)*nStride;
        }
       	if (level<=exLevel) {//Include exchange effect if requested.
          Vec d1 = sectionBeads(i,islice)-sectionBeads(i,islice-nStride);
          Vec d2 = sectionBeads(j,islice)-sectionBeads(j,islice-nStride);
          Vec e1 = sectionBeads(i,islice)-sectionBeads(j,islice-nStride);
          Vec e2 = sectionBeads(j,islice)-sectionBeads(i,islice-nStride);
          cell.pbc(d1); cell.pbc(d2); cell.pbc(e1); cell.pbc(e2);
          double g0  = exp(-0.5*mass*(dot(d1,d1)+dot(d2,d2))*invTauEff);
          double g0X = exp(-0.5*mass*(dot(e1,e1)+dot(e2,e2))*invTauEff);
          if (g0X/g0 > 1e-100) {
            double actX=0.;
       	    if (level<3 && norder>0) {
              Vec svec=delta+prevDelta; double s2=dot(svec,svec)/(q*q);
              if (hasZ) {
                double z=(r-prevR)/q;
                actX+=uk0(q,s2,z*z)*nStride;
              } else {
                actX+=uk0(q,s2)*nStride;
              }
            } else { 
              actX+=u00(q)*nStride;
            }
            double gfull = g0*exp(-action)-g0X*exp(-actX);
            double g0full = g0-g0X;
            if (gfull>0 && g0full >0 && gfull<g0full) {
              action = -log(gfull/g0full);
            } else {
              action = 0;
            }
          }
        }
        deltaAction -= action;
        prevDelta=delta; prevR=r;
      }
    }
  }
  return deltaAction;
}

double PairAction::getActionDifference(const Paths &paths, 
    const VArray &displacement, int nmoving, const IArray &movingIndex, 
    int iFirstSlice, int iLastSlice) {
  const SuperCell& cell=paths.getSuperCell();
  double deltaAction=0;
  for (int iMoving=0; iMoving<nmoving; ++iMoving) {
    const int i=movingIndex(iMoving);
    int jbegin,jend;
    if (i>=ifirst1 && i<ifirst1+npart1) {
      jbegin=ifirst2; jend=ifirst2+npart2;
    } else {
      if (i>=ifirst2 && i<ifirst2+npart2) {
        jbegin=ifirst1; jend=ifirst1+npart1;
      } else {
        continue; //Particle not in this interaction.
      }
    }
    for (int j=jbegin; j<jend; ++j) {
      bool isMoving=false; int jMoving=0;
      for (int k=0;k<nmoving;++k) {
        if (j==movingIndex(k)) {isMoving=true; jMoving=k; break;}
      }
      if (isMoving && i<=j) continue; //Don't double count moving interactions.
      Vec prevDelta =paths(i,iFirstSlice,-1);
      prevDelta-=paths(j,iFirstSlice,-1); cell.pbc(prevDelta);
      Vec prevMovingDelta=prevDelta+displacement(iMoving);
      if (isMoving) prevMovingDelta-=displacement(jMoving);
      cell.pbc(prevMovingDelta);
      double prevR=sqrt(dot(prevDelta,prevDelta));
      double prevMovingR=sqrt(dot(prevMovingDelta,prevMovingDelta));
      for (int islice=iFirstSlice; islice<=iLastSlice; islice++) {
        // Add action for moving beads.
        double action = 0.;
        Vec delta=paths(i,islice);
        delta+=displacement(iMoving);
        delta-=paths(j,islice);
        if (isMoving) delta-=displacement(jMoving);
        cell.pbc(delta);
        double r=sqrt(dot(delta,delta));
        double q=0.5*(r+prevMovingR);
       	if (norder>0) {
          Vec svec=delta-prevMovingDelta; double s2=dot(svec,svec)/(q*q);
          if (hasZ) {
            double z=(r-prevMovingR)/q;
            action+=uk0(q,s2,z*z);
          } else {
            action+=uk0(q,s2);
          }
        } else {
          action+=u00(q);
        }
       	if (exLevel>=0) {//Include exchange effect if requested.
          Vec d1 = paths(i,islice)-paths(i,islice,-1);
          Vec d2 = paths(j,islice)-paths(j,islice,-1);
          Vec e1 = paths(i,islice)-paths(j,islice,-1)+displacement(iMoving);
          Vec e2 = paths(j,islice)-paths(i,islice,-1)-displacement(iMoving);
          if (isMoving) {
            e1 -= displacement(jMoving);
            e2 += displacement(jMoving);
          }
          cell.pbc(d1); cell.pbc(d2); cell.pbc(e1); cell.pbc(e2);
          double g0  = exp(-0.5*mass*(dot(d1,d1)+dot(d2,d2))/tau);
          double g0X = exp(-0.5*mass*(dot(e1,e1)+dot(e2,e2))/tau);
          if (g0X/g0 > 1e-100) {
            double actX=0.;
       	    if (norder>0) {
              Vec svec=delta+prevMovingDelta; double s2=dot(svec,svec)/(q*q);
              if (hasZ) {
                double z=(r-prevMovingR)/q;
                actX+=uk0(q,s2,z*z);
              } else {
                actX+=uk0(q,s2);
              }
            } else { 
              actX+=u00(q);
            }
            double gfull = g0*exp(-action)-g0X*exp(-actX);
            double g0full = g0-g0X;
            if (gfull>0 && g0full >0 && gfull<g0full) {
              action = -log(gfull/g0full);
            } else {
              action = 0;
            }
          }
        }
        deltaAction += action;
        prevMovingDelta=delta; prevMovingR=r;
        // Subtract action for old beads.
        action = 0.;
        delta=paths(i,islice);
        delta-=paths(j,islice);
        cell.pbc(delta);
        r=sqrt(dot(delta,delta));
        q=0.5*(r+prevR);
        if (norder>0) {
          Vec svec=delta-prevDelta;  double s2=dot(svec,svec)/(q*q);
          if (hasZ) {
            double z=(r-prevR)/q;
            action+=uk0(q,s2,z*z);
          } else {
            action+=uk0(q,s2);
          }
        } else {
          action+=u00(q);
        }
       	if (exLevel>=0) {//Include exchange effect if requested.
          Vec d1 = paths(i,islice)-paths(i,islice,-1);
          Vec d2 = paths(j,islice)-paths(j,islice,-1);
          Vec e1 = paths(i,islice)-paths(j,islice,-1);
          Vec e2 = paths(j,islice)-paths(i,islice,-1);
          cell.pbc(d1); cell.pbc(d2); cell.pbc(e1); cell.pbc(e2);
          double g0  = exp(-0.5*mass*(dot(d1,d1)+dot(d2,d2))/tau);
          double g0X = exp(-0.5*mass*(dot(e1,e1)+dot(e2,e2))/tau);
          if (g0X/g0 > 1e-100) {
            double actX=0.;
       	    if (norder>0) {
              Vec svec=delta+prevDelta; double s2=dot(svec,svec)/(q*q);
              if (hasZ) {
                double z=(r-prevR)/q;
                actX+=uk0(q,s2,z*z);
              } else {
                actX+=uk0(q,s2);
              }
            } else { 
              actX+=u00(q);
            }
            double gfull = g0*exp(-action)-g0X*exp(-actX);
            double g0full = g0-g0X;
            if (gfull>0 && g0full >0 && gfull<g0full) {
              action = -log(gfull/g0full);
            } else {
              action = 0;
            }
          }
        }
        deltaAction -= action;
        prevDelta=delta; prevR=r;
      }
    }
  }
  return deltaAction;
}

double PairAction::getTotalAction(const Paths& paths, int level) const {
  return 0;
}

void PairAction::getBeadAction(const Paths& paths, int ipart, int islice,
    double& u, double& utau, double& ulambda, Vec& fm, Vec& fp) const {

  u=utau=ulambda=0; fm=0.; fp=0.;
  int jbegin,jend;
  if (ipart>=ifirst1 && ipart<ifirst1+npart1) {
    jbegin=ifirst2; jend=ifirst2+npart2;
  } else
  if (ipart>=ifirst2 && ipart<ifirst2+npart2) {
    jbegin=ifirst1; jend=ifirst1+npart1;
  } else { 
    return; //Particle not in this interaction.
  }
  SuperCell cell=paths.getSuperCell();
  for (int j=jbegin; j<jend; ++j) {
    if (ipart==j) continue;
    Vec delta=paths(ipart,islice);
    delta-=paths(j,islice);
    cell.pbc(delta);
    double r=sqrt(dot(delta,delta));
    Vec prevDelta=paths(ipart,islice,-1);
    prevDelta-=paths(j,islice,-1);
    cell.pbc(prevDelta);
    double prevR=sqrt(dot(prevDelta,prevDelta));
    double q=0.5*(r+prevR);
    Vec svec=delta-prevDelta; double s2=dot(svec,svec)/(q*q);
    double v,vtau,vq,vs2,vz2,z;
    if (hasZ) { 
      z=(r-prevR)/q;
      uk0CalcDerivatives(q,s2,z*z,v,vtau,vq,vs2,vz2);
    } else {
      uk0CalcDerivatives(q,s2,v,vtau,vq,vs2);
      vz2=0.; z=0;
    }
    if (exLevel>=0) {//Include exchange effect if requested.
      Vec d1 = paths(ipart,islice)-paths(ipart,islice,-1);
      Vec d2 = paths(j,islice)-paths(j,islice,-1);
      Vec e1 = paths(ipart,islice)-paths(j,islice,-1);
      Vec e2 = paths(j,islice)-paths(ipart,islice,-1);
      cell.pbc(d1); cell.pbc(d2); cell.pbc(e1); cell.pbc(e2);
      double g0  = exp(-mass*(dot(d1,d1)+dot(d2,d2))/(2*tau));
      double g0X = exp(-mass*(dot(e1,e1)+dot(e2,e2))/(2*tau));
      if (g0X/g0 > 1e-100) {
        double vX=0., vtauX=0.;
        Vec svec=delta+prevDelta; double s2=dot(svec,svec)/(q*q);
        if (hasZ) {
          double z=(r-prevR)/q;
          uk0CalcDerivatives(q,s2,z*z,vX,vtauX,vq,vs2,vz2);
        } else {
          uk0CalcDerivatives(q,s2,vX,vtauX,vq,vs2);
        }
        double gfull = g0*exp(-v)-g0X*exp(-vX);
        double g0full = g0-g0X;
        if (gfull>0 && g0full >0 && gfull<g0full) {
          double g0tau  = mass*(dot(d1,d1)+dot(d2,d2))/(2*tau*tau)*g0;
          double g0Xtau = mass*(dot(e1,e1)+dot(e2,e2))/(2*tau*tau)*g0X;
          vtau = (g0tau-g0Xtau)/(g0-g0X)
                -((g0tau-g0*vtau)*exp(-v)-(g0Xtau-g0X*vtauX)*exp(-vX))
                /(g0*exp(-v)-g0X*exp(-vX));
          v = -log(gfull/g0full);
        } else {
          vtau = 0; v = 0;
        }
      }
    }
    u += 0.5*v;
    utau += 0.5*vtau;


    // NEEDS TO HAVE EXCHANGE CONTRIBUTION.
    fm -= vq*delta/(2*r) + vs2*(2*svec/(q*q) - s2*delta/(q*r))
         +vz2*z*delta*(2-z)/(q*r);
    // And force contribution from next slice.
    Vec nextDelta=paths(ipart,islice,+1);
    nextDelta-=paths(j,islice,+1);
    cell.pbc(nextDelta);
    double nextR=sqrt(dot(nextDelta,nextDelta));
    q=0.5*(r+nextR);
    svec=delta-nextDelta; s2=dot(svec,svec)/(q*q);
    if (hasZ) { 
      z=(r-nextR)/q;
      uk0CalcDerivatives(q,s2,z*z,v,vtau,vq,vs2,vz2);
    } else {
      uk0CalcDerivatives(q,s2,v,vtau,vq,vs2);
      vz2=0.; z=0.;
    }
    fp -= vq*delta/(2*r) + vs2*(2*svec/(q*q) - s2*delta/(q*r))
         +vz2*z*delta*(2-z)/(q*r);
  }
 
}

double PairAction::u00(double r) const {
  r*=rgridinv; double x=log(r)*logrratioinv;
  int i = (int)x;
  if (r<1) {i=0; x=0;}
  else if (i>ngpts-2) {i=ngpts-2; x=1;}
  else x-=i;
  return (1-x)*ugrid(i,0,0)+x*ugrid(i+1,0,0);
}

double PairAction::uk0(double q, double s2) const {
  q*=rgridinv; double x=log(q)*logrratioinv;
  int i = (int)x;
  if (q<1) {i=0; x=0;}
  else if (i>ngpts-2) {i=ngpts-2; x=1;}
  else x-=i;
  double action=0;
  for (int k=norder; k>=0; k--) {
    action*=s2; action+=(1-x)*ugrid(i,0,k)+x*ugrid(i+1,0,k);
  }
  return action;
}

double PairAction::uk0(double q, double s2, double z2) const {
  q*=rgridinv; double x=log(q)*logrratioinv;
  int i = (int)x;
  if (q<1) {i=0; x=0;}
  else if (i>ngpts-2) {i=ngpts-2; x=1;}
  else x-=i;
  int index=0;
  double action=0;
  for (int k=0; k<=norder; k++) {
    for (int j=0; j<=k; j++) {
      action += ((1-x)*ugrid(i,0,index)+x*ugrid(i+1,0,index))*pow(z2,j)*pow(s2,k-j);
      index++;
    }
  }
  return action;
}

void PairAction::uk0CalcDerivatives(double q, double s2, double &u,
            double &utau, double &uq, double &us2) const {
  q*=rgridinv; double x=log(q)*logrratioinv;
  int i = (int)x;
  if (q<1) {i=0; x=0;}
  else if (i>ngpts-2) {i=ngpts-2; x=1;}
  else x-=i;
  u=utau=uq=us2=0;
  for (int k=norder; k>=0; k--) {
    u*=s2; u+=(1-x)*ugrid(i,0,k)+x*ugrid(i+1,0,k);
    utau*=s2; utau+=(1-x)*ugrid(i,1,k)+x*ugrid(i+1,1,k);
    uq*=s2; uq+=ugrid(i+1,0,k)-ugrid(i,0,k);
  }
  uq*=logrratioinv*rgridinv/q;
  for (int k=norder; k>0; k--) {
    us2*=s2; us2+=k*((1-x)*ugrid(i,0,k)+x*ugrid(i+1,0,k));
  }
}
//
void PairAction::uk0CalcDerivatives(double q, double s2, double z2, double &u,
            double &utau, double &uq, double &us2, double& uz2) const {
  q*=rgridinv; double x=log(q)*logrratioinv;
  int i = (int)x;
  if (q<1) {i=0; x=0;}
  else if (i>ngpts-2) {i=ngpts-2; x=1;}
  else x-=i;
  u=utau=uq=us2=uz2=0.;
  double z2j, s2kminusj,ukj,dukj;
  int index=0;
  for (int k=0; k<=norder; k++) {
    for (int j=0; j<=k; j++) {
      z2j=pow(z2,j);
      s2kminusj=pow(s2,k-j);
      ukj=(1-x)*ugrid(i,0,index)+x*ugrid(i+1,0,index);
      dukj=ugrid(i+1,0,index)-ugrid(i,0,index);

      u+=ukj*z2j*s2kminusj; 
      utau+=( (1-x)*ugrid(i,1,index)+x*ugrid(i+1,1,index) )*z2j*s2kminusj; 

      uz2+=j*ukj*s2kminusj*z2j;
      us2+=(k-j)*ukj*z2j*s2kminusj;
      uq+=z2j*s2kminusj*dukj;

      index++;
    }
  }
  uz2/=z2;
  us2/=s2;
  uq*=logrratioinv*rgridinv/q;
}

void PairAction::write(const std::string &fname,const bool hasZ) const {
  std::string filename=(fname=="")?(species1.name+species2.name):fname;
  std::ofstream ufile((filename+".dmu").c_str()); ufile.precision(8);
  ufile.setf(ufile.scientific,ufile.floatfield); ufile.setf(ufile.showpos);
  std::ofstream efile((filename+".dme").c_str()); efile.precision(8);
  efile.setf(efile.scientific,efile.floatfield); efile.setf(efile.showpos);
  int ndata=1;
  if (hasZ) {
    ndata += norder*(norder+3)/2;
  } else {
    ndata+=norder;
  }
  // write
  for (int i=0; i<ngpts; ++i) {
    double r=(1./rgridinv)*exp(i/logrratioinv);
    ufile << r; efile << r;
    for (int idata=0; idata<ndata; ++idata) {
      ufile << " " << ugrid(i,0,idata);
      efile << " " << ugrid(i,1,idata);
    }
    ufile << std::endl; efile << std::endl;
  }
}
