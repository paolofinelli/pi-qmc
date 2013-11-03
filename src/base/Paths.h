#ifndef __Paths_h_
#define __Paths_h_

class SuperCell;
template <int TDIM> class Beads;
class Permutation;
class LinkSummable;
class ModelState;
#include <config.h>
#include <cstdlib>
#include <blitz/array.h>

/// Class for Paths, including Beads, connectivity (including any Permutation),
/// the time step and SuperCell.
/// Several operations are needed for paths:
/// -# Check out and check in of a section of beads for multi level sampling.
/// -# Looping over all beads to accumulate estimators.
/// -# Accessing single beads with read-only or read-write access.
/// -# Get the global permuation of the particles.
///
/// In parallel simulations with multiple workers, each worker owns
/// some of the slices. Further, the worker will have some other workers' 
/// slices buffered, which it has access to in read-only mode.
/// To check out paths or set the coordinates, it is necessary to know which 
/// slices a path owns. The notion or ownership is also need to perform
/// the sumOverLinks. Note that in parallel simulations is it often 
/// necessary to freeze a slice during MultiLevelSampling in order to
/// prevent extra communication; hence the last slice owned in a 
/// parallel simulation may be treated as read-only by a SectionChooser. 
/// The following methods give information about
/// ownership and allowed sampling
/// - isOwnedSlice
/// - getLowestOwnedSlice
/// - getHighestOwnedSlice
/// - getHigestSampledSlice
///
/// Finally, for parallel simulations it is necessary to shift the
/// slices so that boundaries betweenw workers get sampled.
/// For parallel fermion simulations, beads are stored for two sections,
/// and this condition is flagged by the isDoubleMethod.
/// @version $Revision$
/// @author John Shumway
class Paths {
public:
  /// Constants and typedefs.
  typedef blitz::TinyVector<double,NDIM> Vec;
  typedef blitz::Array<Vec,1> VArray;
  /// Constructor.
  Paths(int npart, int nslice, double tau, const SuperCell& cell);
  /// Destructor.
  virtual ~Paths() {}
  /// Loop over links, calling a LinkSummable object.
  virtual void sumOverLinks(LinkSummable&) const=0;
  /// Get the number of particles.
  int getNPart() const {return npart;}
  /// Get the number of slices.
  int getNSlice() const {return nslice;}
  /// Get the number of slices on this processor.
  //virtual int getNProcSlice(){return nslice;}
  /// Get the number of slices owned by this processor.
  //virtual int getNOwnedSlice() {return nslice;}
  /// Get the temperature for a slice.
  double getTau() const {return tau;}
  /// Get the supercell.
  const SuperCell& getSuperCell() const {return cell;}
  /// Get a reference to a bead.
  virtual Vec& operator()(int ipart, int islice)=0;
  /// Get a const reference to a bead.
  virtual const Vec& operator()(int ipart, int islice) const=0;
  /// Get a reference to a bead by offset.
  virtual Vec& operator()(int ipart, int islice, int istep)=0;
  /// Get a const reference to a bead by offset.
  virtual const Vec&
    operator()(int ipart, int islice, int istep) const=0;
  /// Get a relative displacement a bead by offset.
  virtual Vec
    delta(int ipart, int islice, int istep) const=0;
  /// Get beads.
  virtual void getBeads(int ifirstSlice, Beads<NDIM>& ) const=0;
  /// Get auxialiary bead.
  virtual const void* getAuxBead(int ipart, int islice, int iaux) const=0;
  /// Get auxialiary bead.
  virtual void* getAuxBead(int ipart, int islice, int iaux)=0;
  /// Get a slice.
  virtual void getSlice(int islice, VArray& ) const=0;
  /// Put beads.
  virtual void putBeads(int ifirstSlice,
                        const Beads<NDIM>&, const Permutation&) const=0;
  virtual void putDoubleBeads(
            int ifirstSlice1,Beads<NDIM>&, Permutation&,
            int ifirstSlice2,Beads<NDIM>&, Permutation&) const=0;
  /// Get the global permuation.
  virtual const Permutation& getPermutation() const=0;
  virtual const Permutation& getGlobalPermutation() const=0;
  virtual int getLowestOwnedSlice(bool d) const=0;
  virtual int getHighestOwnedSlice(bool d) const=0;
  virtual int getHighestSampledSlice(int n, bool d) const=0;
  virtual bool isOwnedSlice(int islice) const=0;
  virtual void shift(int ishift)=0;
  virtual void setBuffers() {}
  virtual bool isDouble() const {return false;}
  virtual void clearPermutation()=0;
  ModelState* getModelState() {return modelState;}
  const ModelState* getModelState() const {return modelState;}
  void setModelState(ModelState *m) {modelState=m;}
  bool hasModelState() {return modelState != 0;}
protected:
  /// Number of particles.
  const int npart;
  /// Number of slices.
  const int nslice;
  /// The temperature of one timestep.
  const double tau;
  /// The supercell.
  const SuperCell& cell;

  ModelState *modelState;
};
#endif
