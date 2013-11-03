#ifndef __TimpQPC_h_
#define __TimpQPC_h_
#include "Action.h"
#include <cstdlib>
#include <blitz/array.h>

template <int TDIM> class Beads;
class SectionSamplerInterface;
class DisplaceMoveSampler;
class Species;
class SuperCell;

class MPIManager;

/** Class for calculating the action for a model of a quantum point contact.
  * This is a parameterized model of the repulsive potential for gates
  * with potential @f$ V_g @f$, width w, a gap length l, a distance
  * z above the 2DEG.
  * From Temp (eq. 1) in Semiconductors and Semimetals 35, 113-190 (1992).
  * @f[ V(x,y,z) = f(\frac{2x-l}{2z},\frac{2y+w}{2z})-
  *   f(\frac{2x+l}{2z},\frac{2y+w}{2z}) + f(\frac{2x-l}{2z},\frac{-2y+w}{2z})-
  *   f(\frac{2x+l}{2z},\frac{-2y+w}{2z}) @f]
  * where
  * @f[ f(u,v) = \frac{V_g}{2\pi} \left[ \frac{\pi}{2} -\tan^{-1}(u) 
  * -\tan^{-1}(v)+\tan^{-1}\left(\frac{uv}{\sqrt{1+u^2+v^2}}\right)\right]. @f]
  * We set the default values to w=0.01 um, l=0.03 um, vG=-1V, and z=0.01 um.
  *
  * @version $Revision$
  * @author John Shumway. */
class TimpQPC : public Action {
public:
  /// Typedefs.
  typedef blitz::Array<int,1> IArray;
  /// Constructor by providing the timestep tau.
  TimpQPC(const SuperCell &cell, const Species &species, 
            const double tau, const double width=189.,
            const double length=587., const double vG=-0.03675,
            const double z=189., MPIManager *mpi=0);
  /// Virtual destructor.
  virtual ~TimpQPC() {}
  /// Calculate the difference in action.
  virtual double getActionDifference(const SectionSamplerInterface&,
                                     const int level);
 virtual double getActionDifference(const Paths&, const VArray &displacement,
    int nmoving, const IArray &movingIndex, int iFirstSlice, int iLastSlice);
  /// Calculate the total action.
  virtual double getTotalAction(const Paths&, const int level) const;
  /// Calculate the action and derivatives at a bead.
  virtual void getBeadAction(const Paths&, int ipart, int islice,
    double& u, double& utau, double& ulambda, Vec &fm, Vec &fp) const;
private:
  /// Evaluate the potential at a point.
  double v(double x, double y) const;
  /// Helper functions.
  double f(const double u, const double v) const;
  /// The timestep.
  const double tau;
  /// The width of the barrier electrode.
  const double width;
  /// The length of the QPC gap.
  const double length;
  /// The voltage on the gate.
  const double vG;
  /// The distance from the 2DEG to the gates.
  const double z;
  /// The first particle in this interaction.
  const int ifirst;
  /// The number of particles with this interaction.
  const int npart;
  const double lx,ly;
  /// Pi.
  static const double PI;
};
#endif
