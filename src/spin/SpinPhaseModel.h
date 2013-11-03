//$Id$
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
#ifndef __SpinPhaseModel_h_
#define __SpinPhaseModel_h_

#include <cstdlib>
#include <blitz/array.h>

/** Base class for phase models used in the fixed phase method. 

Suppose we have density matrix of complex funtion, we can write 
@f$ \rho = |\rho|e^{i\phi}  @f$.
We need the derivative of density matrix,
@f[ \nabla\rho = (\nabla|\rho|)e^{i\phi} + i |\rho|e^{i\phi} \nabla \phi  
= \dfrac{\nabla|\rho|}{|\rho|}\rho+i(\nabla\phi)\rho @f]
Divide by @f$\rho@f$ on each side, we get
@f[ \dfrac{\nabla\rho}{\rho} =  \dfrac{\nabla|\rho|}{|\rho|}+i(\nabla\phi) 
= Re(\dfrac{\nabla\rho}{\rho}) + i Im(\dfrac{\nabla\rho}{\rho})@f] 
Now we can get the derivative of the phase.
@f[\boxed{\nabla\phi=Im \left(\dfrac{\nabla\rho}{\rho} \right)  }  @f]    
How to get @f$\frac{\nabla\rho}{\rho}@f$ in the simulation?
@version $Revision$
@author Daejin Shin and John Shumway */
class SpinPhaseModel {
public:
  typedef blitz::TinyVector<double,NDIM> Vec;
  typedef blitz::Array<Vec,1> VArray;
  typedef blitz::TinyVector<double,4> SVec;
  typedef blitz::Array<SVec,1> SArray;
  /// Constructor.
  SpinPhaseModel(const int npart) : phi(0),gradPhi1(npart),gradPhi2(npart),
      sgradPhi1(npart),sgradPhi2(npart),vecPot1(npart),vecPot2(npart) {}
  /// Virtual destructor.
  virtual ~SpinPhaseModel() {}
  /// Evaluate the SpinPhaseModel.
  virtual void evaluate(const VArray&, const VArray&,
                        const SArray&, const SArray&, const int islice)=0;
  /// Get the value of the phase.
  double getPhi() const {return phi;}
  /// Get the value of the gradient of the phase.
  const VArray& getGradPhi(const int i) const {return (i==1)?gradPhi1:gradPhi2;}
  /// Get the value of the spin gradient of the phase.
  const SArray& getSGradPhi(const int i) const {
      return (i==1)?sgradPhi1:sgradPhi2;}
  /// Get the value of the  vector potential.
  const VArray& getVecPot(const int i) const {return (i==1)?vecPot1:vecPot2;}
protected:
  /// The value of the phase.
  double phi;
  /// The gradient of the phase.
  VArray gradPhi1,gradPhi2;
  /// The spin gradient of the phase.
  SArray sgradPhi1,sgradPhi2;
  /// The vector potential.
  VArray vecPot1,vecPot2;
};
#endif
