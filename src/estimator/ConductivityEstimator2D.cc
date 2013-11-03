#include "config.h"
#ifdef ENABLE_MPI
#include <mpi.h>
#endif
#include "ConductivityEstimator2D.h"
#include "action/Action.h"
#include "action/DoubleAction.h"
#include "base/SimulationInfo.h"
#include "util/SuperCell.h"
#include <cstdlib>
#include <blitz/tinyvec.h>
#include "stats/MPIManager.h"

/*
  To do: take into accout the links crossing the boundary of the supercell.
*/

ConductivityEstimator2D::ConductivityEstimator2D(const SimulationInfo&simInfo,
  const double xmin, const double xmax, const double ymin, 
  const double ymax, const int nfreq, const std::string dim1, const std::string dim2, 
  const int nxbin, const int nybin, 
  const int nxdbin, const int nydbin, const int nstride, MPIManager *mpi)
  : BlitzArrayBlkdEst<7>("conductivity2D","dynamic-array/conductivity-2D",
     IVecN(2*nxdbin-1,nxbin,2*nydbin-1,nybin,dim1.length(),dim2.length(),nfreq),
     true),
    npart(simInfo.getNPart()), nslice(simInfo.getNSlice()), 
    nfreq(nfreq), nstride(nstride),
    tau(simInfo.getTau()), tauinv(1./tau), 
    massinv(1./simInfo.getSpecies(0).mass), q(npart),
    nxbin(nxbin), nybin(nybin), nxdbin(nxdbin), nydbin(nydbin),
    xmin(xmin), xmax(xmax), ymin(ymin), ymax(ymax),
    dx((xmax-xmin)/nxbin), dy((ymax-ymin)/nybin), dxinv(1./dx), dyinv(1./dy),
    ax((simInfo.getSuperCell()->a[0])/2), ay((simInfo.getSuperCell()->a[1])/2),
    tolxdbin(2*nxdbin-1), tolydbin(2*nydbin-1),
    dir1(dim1.length()), dir2(dim2.length()),
    temp(tolxdbin,nxbin,tolydbin,nybin,dim1.length(),
         dim2.length(),nslice/nstride),
    j(2,nxbin,nybin,nslice/nstride),
     mpi(mpi) {
  if (dim1.length() == 1) 
    if (dim1=="x") dir1(0) = 0;
    else dir1(0) = 1;
  else dir1=0,1;
  if (dim2.length() == 1)
    if (dim2=="x") dir2(0) = 0;
    else dir2(0) = 1;
  else dir2=0,1;
  fftw_complex *ptrx = (fftw_complex*)j.data();
  int nsliceEff=nslice/nstride;
  fwdx = fftw_plan_many_dft(1, &nsliceEff, 2*nxbin*nybin, 
                            ptrx, 0, 1, nsliceEff,
                            ptrx, 0, 1, nsliceEff,
                            FFTW_FORWARD, FFTW_MEASURE);
  for (int i=0; i<npart; ++i) q(i)=simInfo.getPartSpecies(i).charge;
}

ConductivityEstimator2D::~ConductivityEstimator2D() {
  fftw_destroy_plan(fwdx);
}

void ConductivityEstimator2D::initCalc(const int lnslice, const int firstSlice) {
  temp=0;
  j=0;
}

void ConductivityEstimator2D::handleLink(const Vec& start, const Vec& end, 
                                    const int ipart, const int islice, const Paths& paths) {
  if ((start[0]<xmin && end[0]<xmin) || (start[0]>xmax && end[0]>xmax)) return;
  if ((start[1]<ymin && end[1]<ymin) || (start[1]>ymax && end[1]>ymax)) return;
  // Exclude the links crossing the boundary of supercell.
  if (end[0] - start[0] > ax || start[0] - end[0] > ax) return;
  if (end[1] - start[1] > ay || start[1] - end[1] > ay) return;
  int ibin = ((int)((start[0] - xmin)*dxinv));
  if (ibin < 0) {ibin = 0;}
  else if (ibin > nxbin-1) {ibin = nxbin;}
  int jbin = ((int)((end[0] - xmin)*dxinv));
  if (jbin < 0) {jbin = 0;}
  else if (jbin > nxbin-1) {jbin = nxbin;}
  if (ibin!=jbin) {
    double k = (end[1] - start[1])/(end[0] - start[0]);
    int idir = (end[0]-start[0]>0)?1:(-1);
    if (idir < 0) {ibin-=1; jbin-=1;}
    for (int i=ibin; i*idir<jbin*idir; i+=idir) {
      double x = xmin + (i+1)*dx;
      // The y index in matrix here is not the reverse of y coordinate.
      int ybin = (int)((start[1] + k*(x - start[0]) - ymin)*dyinv);
      if (ybin > nybin-1 || ybin < 0) continue;
      j(0,i,ybin,islice/nstride)+=idir*q(ipart);
    }
  }
  ibin = ((int)((start[1] - ymin)*dyinv));
  if (ibin < 0) {ibin = 0;}
  else if (ibin > nybin-1) {ibin = nybin;}
  jbin = ((int)((end[1] - ymin)*dyinv));
  if (jbin < 0) {jbin = 0;}
  else if (jbin > nybin-1) {jbin = nybin;}
  if (ibin!=jbin) {
    double kinv = (end[0] - start[0])/(end[1] - start[1]);
    int idir = (end[1]-start[1]>0)?1:(-1);
    if (idir < 0) {ibin-=1; jbin-=1;}
    for (int i=ibin; i*idir<jbin*idir; i+=idir) {
      double y = ymin + (i+1)*dy;
      int xbin = (int)((start[0] + kinv*(y - start[1]) - xmin)*dxinv);
      if (xbin > nxbin-1 || xbin < 0) continue;
      j(1,xbin,i,islice/nstride)+=idir*q(ipart);
    }
  }
} 

void ConductivityEstimator2D::endCalc(const int lnslice) {
  blitz::Range allSlice = blitz::Range::all();
  blitz::Range allBin = blitz::Range::all();
  blitz::Range alldir = blitz::Range::all();
  // First move all data to 1st worker.
  int workerID = (mpi)?mpi->getWorkerID():0;
#ifdef ENABLE_MPI
  CArray4 j_tmp(j.shape());
  j_tmp=0;
  if (mpi) {
    mpi->getWorkerComm().Reduce(j.data(), j_tmp.data(), 2*2*nxbin*nybin*nslice/nstride,
                                MPI::DOUBLE, MPI::SUM, 0);
    j=j_tmp;
  }
#endif
  // Calculate correlation function using FFT's.
  if (workerID==0) {
    fftw_execute(fwdx);
    int ndim1=dir1.size();
    int ndim2=dir2.size();
    for(int idim1=0; idim1<ndim1; ++idim1) 
      for(int idim2=0; idim2<ndim2; ++idim2) 
        for (int ibinx=0; ibinx<nxbin; ++ibinx) {
          for (int jdbinx=0; jdbinx<nxdbin; ++jdbinx) {
            int jbinx = (ibinx + jdbinx) % nxbin;
            for(int ibiny=0; ibiny<nybin; ++ibiny) {
              for(int jdbiny=0; jdbiny<nydbin; ++jdbiny) {
            // j1-j2
                int jbiny = (ibiny + jdbiny) % nybin;
                temp(jdbinx, ibinx, jdbiny, ibiny, idim1, idim2, allSlice)
                 = conj(j(dir1(idim1),ibinx,ibiny,allSlice)) * 
                        j(dir2(idim2),jbinx,jbiny,allSlice);
                jbiny = (ibiny - jdbiny + nybin) % nybin;
                temp(jdbinx, ibinx, (tolydbin-jdbiny)%tolydbin, ibiny, idim1, idim2, 
                     allSlice)
                 = conj(j(dir1(idim1),ibinx,ibiny,allSlice)) * 
                        j(dir2(idim2),jbinx,jbiny,allSlice);
              }
            }
            jbinx = (ibinx - jdbinx + nxbin) % nxbin;
            for(int ibiny=0; ibiny<nybin; ++ibiny) {
              for(int jdbiny=0; jdbiny<nydbin; ++jdbiny) {
            // j1-j2
                int jbiny = (ibiny + jdbiny) % nybin;
                temp((tolxdbin-jdbinx)%tolxdbin, ibinx, jdbiny, ibiny, idim1, idim2, 
                     allSlice)
                 = conj(j(dir1(idim1),ibinx,ibiny,allSlice)) * 
                        j(dir2(idim2),jbinx,jbiny,allSlice);
                jbiny = (ibiny - jdbiny + nybin) % nybin;
                temp((tolxdbin-jdbinx)%tolxdbin, ibinx, (tolydbin-jdbiny)%tolydbin, 
                     ibiny, idim1, idim2, allSlice)
                 = conj(j(dir1(idim1),ibinx,ibiny,allSlice)) * 
                        j(dir2(idim2),jbinx,jbiny,allSlice);
              }
            }
          }
        }

    value += real(temp(allBin, allBin, allBin, allBin, alldir, alldir, 
                  blitz::Range(0,nfreq-1))) / (tau*nslice);
    norm+=1;
  }
}
