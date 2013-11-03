// $Id$
/*  Copyright (C) 2009 John B. Shumway, Jr.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.  */
#ifndef __DensCountEstimator_h_
#define __DensCountEstimator_h_
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#ifdef ENABLE_MPI
#include <mpi.h>
#endif
#include "action/Action.h"
#include "base/LinkSummable.h"
#include "base/Paths.h"
#include "base/SimulationInfo.h"
#include "base/Species.h"
#include "stats/BlitzArrayBlkdEst.h"
#include "stats/MPIManager.h"
#include "util/SuperCell.h"
#include <cstdlib>
#include <vector>
#include <blitz/array.h>
#include <blitz/tinyvec-et.h>
class Distance;
/** Calculates single particle densities for many different geometries.
 *  Implements several options for studying fluctations.
 *  @version $Revision$
 *  @author John Shumway  */
class DensCountEstimator : public LinkSummable, 
                           public BlitzArrayBlkdEst<NDIM+1> {
public:
  typedef blitz::TinyVector<double,NDIM> Vec;
  typedef blitz::TinyVector<int, NDIM> IVec;
  typedef std::vector<Distance*> DistArray;
  typedef blitz::Array<int,NDIM> IArrayNDIM;

  /// Constructor.
  DensCountEstimator(const SimulationInfo& simInfo, const std::string& name,
      const Species *s, const Vec &min, const Vec &max, const IVec &nbin,
      const IVecN &nbinN, const DistArray &dist, MPIManager *mpi); 

  /// Virtual destructor.
  virtual ~DensCountEstimator();
  
  /// Initialize the calculation.
  virtual void initCalc(const int nslice, const int firstSlice);

  /// Add contribution from a link.
  virtual void handleLink(const Vec& start, const Vec& end,
      const int ipart, const int islice, const Paths &paths);
  
  /// Finalize the calculation.
  virtual void endCalc(const int nslice);
  
  /// Clear value of estimator.
  virtual void reset();

  /// Evaluate for Paths configuration.
  virtual void evaluate(const Paths& paths);

private:
  const Vec min;
  const Vec deltaInv;
  const IVec nbin;
  const IVecN nbinN;
  const DistArray dist;
  SuperCell cell;
  ArrayN temp;
  IArrayNDIM count;
  int ifirst, npart;
  MPIManager *mpi;
};
#endif
