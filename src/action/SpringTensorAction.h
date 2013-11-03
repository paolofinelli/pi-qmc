// $Id$
/*  Copyright (C) 2004-2006 John B. Shumway, Jr.

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
#ifndef __SpringTensorAction_h_
#define __SpringTensorAction_h_
class SectionSamplerInterface;class DisplaceMoveSampler;
class Paths;
class SimulationInfo;
#include "Action.h"
#include <cstdlib>
#include <blitz/array.h>

/** Class for calculating the free particle ``spring''  for an anisotropic
  * mass action.
  * @f[ u(r,r';\tau) = \frac{(x-x')^2+(y-y')^2}{4\lambda_{xy}\tau}
  *                  + \frac{(z-z')^2}{4\lambda_{z}\tau}  @f]
  * @bug May need to include multiple images.  This isn't implemented
  *      now, and probably isn't a concern in practical simulations.
  * @version $Revision$
  * @author John Shumway. */
class SpringTensorAction : public Action {
public:
  /// Typedefs.
  typedef blitz::Array<int,1> IArray;
  typedef blitz::Array<Vec,1> VArray;
  /// Construct by providing simulation info.
  SpringTensorAction(const SimulationInfo& simInfo);
  /// Virtual destructor.
  virtual ~SpringTensorAction() {}
  /// Calculate the difference in action.
  virtual double getActionDifference(const SectionSamplerInterface&,
                                     int level);
  /// Calculate the total action.
  virtual double getTotalAction(const Paths&, const int level) const;
  /// Calculate action and derivatives at a bead (defaults to no
  /// contribution).
  virtual void getBeadAction(const Paths&, const int ipart, const int islice,
    double& u, double& utau, double& ulambda, Vec& fm, Vec& fp) const;
private:
  /// The inverse particle mass, @f$\lambda=1/2m@f$.
  VArray lambda;
  /// The timestep.
  const double tau;
};
#endif
