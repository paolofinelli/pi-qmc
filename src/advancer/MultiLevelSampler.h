#ifndef __MultiLevelSampler_h_
#define __MultiLevelSampler_h_
template<int TDIM> class Beads;
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
class SuperCell;
class Action;
class Mover;
class Paths;
class SectionChooser;
class ParticleChooser;
class PermutationChooser;
class Permutation;
class AccRejEstimator;
class BeadFactory;
#include "algorithm/Algorithm.h"
#include "SectionSamplerInterface.h"
#include <vector>
#include <cstdlib>
#include <blitz/array.h>
#include <iostream>

/// Class for multilevel sampling of beads.
class MultiLevelSampler: public Algorithm, public SectionSamplerInterface {

public:
    typedef blitz::Array<int, 1> IArray;
    MultiLevelSampler(const int nmoving, Paths&, SectionChooser&,
            ParticleChooser*, PermutationChooser*, Mover&, Action*,
            const int nrepeat, const BeadFactory&, const bool delayedRejection,
            const double defaultFactor, double newFactor, bool);
    virtual ~MultiLevelSampler();
    virtual void run();

    int getNLevel() const {
        return nlevel;
    }

    using SectionSamplerInterface::getMovingBeads;

    virtual Beads<NDIM>& getMovingBeads() {
        return *movingBeads;
    }

    const virtual Beads<NDIM>& getMovingBeads() const {
        return *movingBeads;
    }

    virtual Beads<NDIM>& getMovingBeads(int i) {
        return *movingBeads;
    }

    const virtual Beads<NDIM>& getMovingBeads(const int i) const {
        return *movingBeads;
    }

    virtual int getFirstSliceIndex() const;

    const Beads<NDIM>& getRejectedBeads() const {
        return *rejectedBeads;
    }

    Beads<NDIM>& getRejectedBeads() {
        return *rejectedBeads;
    }

    double getFactor() {
        return factor;
    }

    using SectionSamplerInterface::getSectionBeads;

    virtual Beads<NDIM>& getSectionBeads() {
        return *sectionBeads;
    }

    const virtual Beads<NDIM>& getSectionBeads() const {
        return *sectionBeads;
    }

    virtual Beads<NDIM>& getSectionBeads(int i) {
        return *sectionBeads;
    }

    const virtual Beads<NDIM>& getSectionBeads(const int i) const {
        return *sectionBeads;
    }

    using SectionSamplerInterface::getMovingIndex;

    IArray& getMovingIndex() {
        return *movingIndex;
    }

    const IArray& getMovingIndex() const {
        return *movingIndex;
    }

    IArray& getMovingIndex(int i) {
        return *movingIndex;
    }

    const IArray& getMovingIndex(const int i) const {
        return *movingIndex;
    }

    virtual bool isSamplingBoth() const {
        return false;
    }

    static const int ALL_LEVELS = -1;
    /// Set the action function for a level, or default to level=ALL_LEVELS.
    void setAction(Action*, const int level = ALL_LEVELS);

    /// Get a const reference to the SuperCell.
    const virtual SuperCell& getSuperCell() const {
        return cell;
    }
    /// Get a constant reference to the paths.
    const Paths& getPaths() const {
        return paths;
    }
    /// Get a pointer to the accept/reject statistic estimator.
    /// (You are responsible for deleting this new object.)
    virtual AccRejEstimator* getAccRejEstimator(const std::string& name);
    /// Get const reference to the SectionChooser.
    const SectionChooser& getSectionChooser() const {
        return sectionChooser;
    }
protected:
    /// Attempt a move on the beads.
    bool tryMove(double initialTranProb);
    /// Number of levels.
    const int nlevel;
    /// Number of moving particles.
    const int nmoving;
    /// Reference to all beads in the section.
    Beads<NDIM> *sectionBeads;
    /// Reference the permutation of the in the section.
    Permutation *sectionPermutation;
    /// Storage for the moving beads.
    Beads<NDIM> *movingBeads;
    /// Algorithm to select trial move for the beads.
    Mover& mover;
    /// Supercell.
    const SuperCell& cell;
    /// Action function.
    Action *action;
    /// Index of moving particles.
    IArray *movingIndex;
    /// More indicies for moving particles.
    IArray identityIndex, pMovingIndex;
    /// The algorithm for selecting the particles to move.
    ParticleChooser* particleChooser;
    /// The algorithm for selecting the permutation.
    PermutationChooser* permutationChooser;
    /// Reference to the algorithm that selected the section.
    SectionChooser& sectionChooser;
    /// Reference to the paths (only needed for fixed node reference point).
    const Paths& paths;
    /// AccRejEstimator.
    AccRejEstimator* accRejEst;
    /// Number of times to repeat.
    const int nrepeat;
    ///flag for delayed rejection
    const bool delayedRejection;
    Beads<NDIM> *rejectedBeads;
    double newFactor;
    const double defaultFactor;
    double factor;
    int shouldDeletePermutationChooser;
};
#endif
