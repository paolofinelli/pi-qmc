#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include "Spin4DPhase.h"
#include "base/SimulationInfo.h"
#include "base/Species.h"
#include "util/PeriodicGaussian.h"
#include "util/SuperCell.h"
#include <cstdlib>
#include <blitz/tinyvec-et.h>
#include <blitz/tinymat.h>

#define ZGETRF_F77 F77_FUNC(zgetrf,ZGETRF)
extern "C" void ZGETRF_F77(const int*, const int*, std::complex<double>*,
                           const int*, const int*, int*);
#define ZGETRI_F77 F77_FUNC(zgetri,ZGETRI)
extern "C" void ZGETRI_F77(const int*, std::complex<double>*, const int*, 
                     const int*, std::complex<double>*, const int*, int*);

Spin4DPhase::Spin4DPhase(const SimulationInfo &simInfo,
  const Species &species, const double t,  const double bx, const double by,
  const double bz, const double gmubs, const int maxlevel)
  : PhaseModel(simInfo.getNPart()), tau(simInfo.getTau()), 
    charge(species.charge), temperature(t), bx(bx), by(by), bz(bz), 
    gmubs(gmubs), tanhx(tanh(0.5*gmubs*bx/t)), tanhy(tanh(0.5*gmubs*by/t)),
    tanhz(tanh(0.5*gmubs*bz/t)), npart(species.count),
    ifirst(species.ifirst),
    matrix((1 << maxlevel) + 1),
    gradmat1(npart,npart), gradmat2(npart,npart),
    ipiv(npart), lwork(npart*npart), work(lwork), f(1)/*f(1.0/(16.0*pi*pi))*/ {
  std::cout << "Spin4DPhase with t=" << temperature << ", b=" << bx << " "
	    << by << " " << bz << " " 
            << ", gmubs=" << gmubs << std::endl;
  for (unsigned int i=0; i<matrix.size(); ++i) {
    matrix[i] = new Matrix(npart,npart,ColMajor());
  }
}

Spin4DPhase::~Spin4DPhase() {
  for (unsigned int i=0; i<matrix.size(); ++i) delete matrix[i];
}

const double Spin4DPhase::pi(acos(-1.0));

void Spin4DPhase::evaluate(const VArray &r1, const VArray &r2, 
                          const int islice) {
#if NDIM==4 
  gradPhi1=0; gradPhi2=0; vecPot1=0; vecPot2=0; 
  for (int ipart=0; ipart<npart; ++ipart) {
    SVec s1=r1(ipart+ifirst);
    SVec s2=r2(ipart+ifirst);
    Complex rho=0;
    C4Vec gradrho1, gradrho2;
    // Identity contribution to density matrix.
    rho += Complex( s1[0]*s2[0]+s1[1]*s2[1]+s1[2]*s2[2]+s1[3]*s2[3],
                   -s1[0]*s2[3]+s1[1]*s2[2]-s1[2]*s2[1]+s1[3]*s2[0]);
    gradrho1[0]=Complex(+s2[0],-s2[3]);
    gradrho1[1]=Complex(+s2[1],+s2[2]);
    gradrho1[2]=Complex(+s2[2],-s2[1]);
    gradrho1[3]=Complex(+s2[3],+s2[0]);
    gradrho2[0]=Complex(+s1[0],+s1[3]);
    gradrho2[1]=Complex(+s1[1],-s1[2]);
    gradrho2[2]=Complex(+s1[2],+s1[1]);
    gradrho2[3]=Complex(+s1[3],-s1[0]);
    // Z-direction contribution to density matrix.
    rho += Complex( s1[0]*s2[0]-s1[1]*s2[1]-s1[2]*s2[2]+s1[3]*s2[3],
                   -s1[0]*s2[3]-s1[1]*s2[2]+s1[2]*s2[1]+s1[3]*s2[0])*tanhz;
    gradrho1[0]+=Complex(+s2[0],-s2[3])*tanhz;
    gradrho1[1]+=Complex(-s2[1],-s2[2])*tanhz;
    gradrho1[2]+=Complex(-s2[2],+s2[1])*tanhz;
    gradrho1[3]+=Complex(+s2[3],+s2[0])*tanhz;
    gradrho2[0]+=Complex(+s1[0],+s1[3])*tanhz;
    gradrho2[1]+=Complex(-s1[1],+s1[2])*tanhz;
    gradrho2[2]+=Complex(-s1[2],-s1[1])*tanhz;
    gradrho2[3]+=Complex(+s1[3],-s1[0])*tanhz;

    // X-direction contribution to density matrix.
    rho += Complex( s1[0]*s2[0]-s1[1]*s2[1]-s1[2]*s2[2]+s1[3]*s2[3],
                   -s1[0]*s2[3]-s1[1]*s2[2]+s1[2]*s2[1]+s1[3]*s2[0])*tanhx;
    gradrho1[0]+=Complex(+s2[0],-s2[3])*tanhx;
    gradrho1[1]+=Complex(-s2[1],-s2[2])*tanhx;
    gradrho1[2]+=Complex(-s2[2],+s2[1])*tanhx;
    gradrho1[3]+=Complex(+s2[3],+s2[0])*tanhx;
    gradrho2[0]+=Complex(+s1[0],+s1[3])*tanhx;
    gradrho2[1]+=Complex(-s1[1],+s1[2])*tanhx;
    gradrho2[2]+=Complex(-s1[2],-s1[1])*tanhx;
    gradrho2[3]+=Complex(+s1[3],-s1[0])*tanhx;

    // Y-direction contribution to density matrix.
    rho += Complex( s1[0]*s2[0]-s1[1]*s2[1]-s1[2]*s2[2]+s1[3]*s2[3],
                   -s1[0]*s2[3]-s1[1]*s2[2]+s1[2]*s2[1]+s1[3]*s2[0])*tanhy;
    gradrho1[0]+=Complex(+s2[0],-s2[3])*tanhy;
    gradrho1[1]+=Complex(-s2[1],-s2[2])*tanhy;
    gradrho1[2]+=Complex(-s2[2],+s2[1])*tanhy;
    gradrho1[3]+=Complex(+s2[3],+s2[0])*tanhy;
    gradrho2[0]+=Complex(+s1[0],+s1[3])*tanhy;
    gradrho2[1]+=Complex(-s1[1],+s1[2])*tanhy;
    gradrho2[2]+=Complex(-s1[2],-s1[1])*tanhy;
    gradrho2[3]+=Complex(+s1[3],-s1[0])*tanhy;

    // Phase gradient is the imaginary part of the log gradient.
    for (int i=0; i<4; ++i) {
      gradPhi1(ipart+ifirst)[i]=imag(gradrho1[i]/rho);
      gradPhi2(ipart+ifirst)[i]=imag(gradrho2[i]/rho);
    }
    /* // Density matrix is dot(s1,s2)+i*dot(s1,x2)=dot(s1,s2)+i*dot(x1,s2)
    SVec s1=r1(ipart+ifirst);
    SVec s2=r2(ipart+ifirst);
    SVec x1=SVec(s1[3],s1[2],-s1[1],-s1[0]);
    SVec x2=SVec(-s2[3],-s2[2],s2[1],s2[0]);
    s1[0]*=rhouu; s1[1]*=rhodd; s1[2]*=rhodd; s1[3]*=rhouu;
//s1[1]=s1[2]=s2[1]=s2[2]=0;
//    SVec x1=SVec(s1[3],0,0,-s1[0]);
//    SVec x2=SVec(-s2[3],0,0,s2[0]);
    Complex rhoinv=1.0/Complex(dot(s1,s2),dot(s1,x2));
    s2[0]*=rhouu; s2[1]*=rhodd; s2[2]*=rhodd; s2[3]*=rhouu;
    x1[0]*=rhouu; x1[1]*=rhodd; x1[2]*=rhodd; x1[3]*=rhouu;
    x2[0]*=rhouu; x2[1]*=rhodd; x2[2]*=rhodd; x2[3]*=rhouu;
    for (int i=0; i<4; ++i) {
      gradPhi1(ipart+ifirst)[i]=imag(Complex(s2[i],x2[i])*rhoinv);
      gradPhi2(ipart+ifirst)[i]=imag(Complex(s1[i],x1[i])*rhoinv);
    } */
  }
#endif
}
