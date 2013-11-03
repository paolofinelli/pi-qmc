#ifndef __ConductivityEstimator2D_h__
#define __ConductivityEstimator2D_h__
#include "stats/BlitzArrayBlkdEst.h"
#include "base/LinkSummable.h"
#include "base/Paths.h"
#include <fftw3.h>
class Paths;
class SimulationInfo;
class MPIManager;

class ConductivityEstimator2D : public BlitzArrayBlkdEst<7>, public LinkSummable {
public:
  typedef blitz::Array<std::complex<double>,7> CArray7;
  typedef blitz::Array<std::complex<double>,4> CArray4;
  typedef blitz::Array<int,1> IArray;
  typedef blitz::Array<double,1> Array;
  /// Constructor.
  ConductivityEstimator2D(const SimulationInfo& simInfo, const double xmin, const double xmax, 
    const double ymin, const double ymax,  const int nfreq, const std::string dim1,
    const std::string dim2, const int nxbin, const int nybin,
    const int nxdbin, const int nydbin, const int nstride, MPIManager *mpi);
  /// Virtual destructor.
  virtual ~ConductivityEstimator2D();
  /// Initialize the calculation.
  virtual void initCalc(const int nslice, const int firstSlice);
  /// Add contribution from a link.
  virtual void handleLink(const blitz::TinyVector<double,NDIM>& start,
                          const blitz::TinyVector<double,NDIM>& end,
                          const int ipart, const int islice, const Paths&);
  /// Finalize the calculation.
  virtual void endCalc(const int nslice);
  /// Clear value of the estimator.
  virtual void reset() {}
  /// Evaluate for Paths configuration.
  virtual void evaluate(const Paths& paths) {paths.sumOverLinks(*this);}
private:
  const int npart, nslice, nfreq, nstride;
  const double tau, tauinv, massinv;
  Array q;
  const int nxbin, nybin, nxdbin, nydbin;
  const double xmin, xmax, ymin, ymax;
  const double dx, dy, dxinv, dyinv; 
  const double ax, ay;
  const int tolxdbin, tolydbin;
  IArray dir1, dir2;
  CArray7 temp;
  CArray4 j;
  MPIManager *mpi;
  fftw_plan fwdx;
};

#endif
