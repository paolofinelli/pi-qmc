// $Id$
/*  Copyright (C) 2008-2009 John B. Shumway, Jr.

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
#ifndef __TradEwaldSum
#define __TradEwaldSum
class MultiLevelSampler;
class SectionChooser;
class Paths;
class SuperCell;
#include <vector>
#include <cstdlib>
#include <blitz/array.h>
#include <blitz/tinyvec.h>
#include <complex>
#include "EwaldSum.h"

/** Class for creating and evaluating Ewald sums.
The traditional (and most common)
Ewald summation technique splits a sum of @f$1/r@f$ 
potentials into short-range real space and long-range k-space terms,
@f[
\newcommand{\bfr}{\mathbf{r}}
\newcommand{\bfk}{\mathbf{k}}
\sum_{i<j} \frac{q_iq_j}{|\bfr_{ij}|}
=\sum_{i<j}\frac{q_iq_j\operatorname{erfc}(\kappa|\bfr_{ij}|)}{|\bfr_{ij}|}
+\frac{2\pi}{V}\sum_{\bfk\ne 0}\frac{e^{-\frac{|\bfk|^2}{4\kappa^2}}}{|\bfk|^2}
  \left|\sum_j q_j e^{i\bfk\cdot\bfr_j}\right|^2
-\frac{\kappa}{\sqrt\pi}\sum_j q_j^2
-\frac{2\pi}{3V}\left|\sum_j q_j\bfr_j\right|^2
@f]
Thus the following quantity is zero,
@f[
\newcommand{\bfr}{\mathbf{r}}
\newcommand{\bfk}{\mathbf{k}}
0=-\sum_{i<j}\frac{q_iq_j\operatorname{erf}(\kappa|\bfr_{ij}|)}{|\bfr_{ij}|}
+\frac{2\pi}{V}\sum_{\bfk\ne 0}\frac{e^{-\frac{|\bfk|^2}{4\kappa^2}}}{|\bfk|^2}
  \left|\sum_j q_j e^{i\bfk\cdot\bfr_j}\right|^2
-\frac{\kappa}{\sqrt\pi}\sum_j q_j^2
-\frac{2\pi}{3V}\left|\sum_j q_j\bfr_j\right|^2,
@f]
For a charged system such as the electron gas, there is an additional 
constant term,
@f[
-\frac{\pi}{2\kappa^2 V}\left|\sum_j q_j \right|^2.
@f]
@todo Make a new implementation using optimized Ewald breakup, as described
in <a href="http://dx.doi.org/10.1006/jcph.1995.1054">
Natoli and Ceperley, J. Comp. Phys. <b>117</b>, 171-178 (1995).</a>
@bug Only works for NDIM=3.
@version $Revision$
@author John Shumway. */
class TradEwaldSum : public EwaldSum {
public:
  /// Constructor calcuates the k-vectors for a given rcut and kcut.
  TradEwaldSum(const SuperCell&, const int npart, 
	       const double rcut, const double kcut, const double kappa);
  /// Virtual destructor.
  virtual ~TradEwaldSum();
  /// Returns @f$ f(r) @f$, used to cancel tails on actions or potentials.
  virtual double evalFR(const double r) const;
  /// Returns @f$ f(r\rightarrow 0) @f$, used in evalSelfEnergy.
  virtual double evalFR0() const;
  /// Returns @f$ f(k) @f$, used to set up vk array.
  virtual double evalFK(const double k) const;
  /// Returns @f$ f(k\rightarrow 0) @f$, used in evalSelfEnergy for interaction
  /// with neutralizing background if system has a net charge.
  virtual double evalFK0() const;
private:
  /// Screening parameter.
  const double kappa;
};
#endif
