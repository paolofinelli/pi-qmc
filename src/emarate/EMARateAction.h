#ifndef __EMARateAction_h_
#define __EMARateAction_h_

#include "action/Action.h"
#include "base/Beads.h"
#include <cstdlib>
#include <blitz/array.h>
#include <vector>

class MultiLevelSampler;
class SectionSamplerInterface;
class DisplaceMoveSampler;
class Paths;
class SimulationInfo;
class Species;
class CoulombLinkAction;

/** Class to modify the action to allow for electron-hole recombination.
 * To sample recombination rates, we work with a larger ensemble that
 * contains recombining paths. This larger ensemble can be described by
 * an additional action term,
 * @f[ S_C = -\hbar\ln\left(1+Ce^{-\frac{S_E'-S_E}{\hbar}}\right), @f]
 * where
 * @f[ S_E'-S_E = \frac{m_h|r_{e,0}-r_{h,N_{T}-1}|^2}{2\Delta\tau}
 *              + \frac{m_h|r_{e,1}-r_{h,0}|^2}{2\Delta\tau}
 *              - \frac{m_h|r_{h,0}-r_{h,N_{T}-1}|^2}{2\Delta\tau}
 *              - \frac{m_h|r_{e,1}-r_{e,0}|^2}{2\Delta\tau}. @f]
 * @version $Revision$
 * @author John Shumway */

class EMARateAction : public Action {
public:
    /// Typedefs.
    typedef blitz::Array<int,1> IArray;
    typedef blitz::Array<double,1> Array;
    typedef blitz::Array<bool,1> BArray;

    EMARateAction(const SimulationInfo&, const Species*, const Species*,
            double C);
    virtual ~EMARateAction();

    void includeCoulombContribution(double epsilon, int norder);

    virtual double getActionDifference(const SectionSamplerInterface&,
            const int level);
    /// Calculate the difference in action.
    virtual double getActionDifference(const Paths&, const VArray &displacement,
            int nmoving, const IArray &movingIndex, int iFirstSlice, int iLastSlice);
    /// Calculate the total action.
    virtual double getTotalAction(const Paths&, const int level) const;
    /// Calculate action and derivatives at a bead (defaults to no
    /// contribution).
    virtual void getBeadAction(const Paths&, const int ipart, const int islice,
            double& u, double& utau, double& ulambda, Vec& fm, Vec& fp) const;

private:
    /// The inverse of the timestep.
    const double invTau;
    const Species* species1;
    const Species* species2;
    const int index1;
    const int index2;
    /// The weight parameter "C" used to optimize sampling.
    const double C;
    Vec mass1;
    Vec mass2;
    /// The total number of slices in the path.
    const int nPathSlice;
    bool hasCoulomb;
    CoulombLinkAction *coulomb;
    EMARateAction::Vec getMovingPosition(int npart, int nslice, int nMoving,
            const IArray &index,
            const Beads<NDIM> &sectionBeads,
            const Beads<NDIM> &movingBeads);
    bool isCenteredOnSliceZero(const SectionSamplerInterface & sampler, const int nSlice);
};
#endif
