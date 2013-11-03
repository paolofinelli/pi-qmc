#ifndef __EMARateMover_h_
#define __EMARateMover_h_
#include "advancer/mover/Mover.h"
#include "advancer/PermutationChooser.h"
#include "advancer/ParticleChooser.h"
#include "base/Beads.h"
#include <cstdlib>
#include <blitz/array.h>
#include <vector>
class SimulationInfo;
class PeriodicGaussian;
class Species;

class EMARateMover : public Mover,
                     public PermutationChooser,
                     public ParticleChooser {
public:
    typedef blitz::Array<int,1> IArray;
    typedef blitz::Array<double,1> Array;
    typedef blitz::Array<PeriodicGaussian*,3> PGArray;
    typedef blitz::TinyVector<double,NDIM> Vec;
    EMARateMover(double tau,
            const Species *species1, const Species *species2,
            int maxlevel, double C);
    virtual ~EMARateMover();
    /// Move the samplers moving beads for a given level, returning
    /// the probability for the old move divided by the probability for the
    /// new move.
    virtual double makeMove(MultiLevelSampler&, const int level);
    virtual double makeDelayedMove(MultiLevelSampler&, const int level) ;
    virtual double getForwardProb() {return forwardProb;}
    virtual void chooseParticles() {}

    bool chooseDiagonalOrRadiating(const Beads<NDIM> &movingBeads,
            int nSlice, const SuperCell&);
    double calculateDiagonalProbability(const Beads<NDIM>& movingBeads,
            int nSlice, const SuperCell&);
    double calculateRadiatingProbability(const Beads<NDIM> &movingBeads,
            int nSlice, const SuperCell&);
    void moveBeads(int nStride, int nSlice, Beads<NDIM> &movingBeads,
            const SuperCell &cell);
    void sampleRadiating(int nStride, int nSlice, Beads<NDIM> & movingBeads);
    void sampleDiagonal(int nStride, int nSlice,
            Beads<NDIM> &movingBeads, const SuperCell &cell);
    double calculateTransitionProbability(int nStride,
            const Beads<NDIM>& movingBeads, const Beads<NDIM> &sectionBeads,
            int nSlice, const SuperCell &cell) const;

private:
    double tau;
    Vec lambda1;
    Vec lambda2;
    double forwardProb;
    bool isSamplingRadiating;
    double earlierTransitions;
    const double C;
    const int index1;
    const int index2;

protected:
    virtual double getRandomNumber() const;
    virtual void makeGaussianRandomNumbers(blitz::Array<Vec,1> &gaussRand);
};
#endif
