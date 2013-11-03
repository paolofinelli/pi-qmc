#ifndef __ModelState_h_
#define __ModelState_h_

#include <iostream>
#include <string>
class MPIManager;
#include "stats/PartitionWeight.h"

///Base class for model state.
/// @author John Shumway and Jianheng Liu
class ModelState : public PartitionWeight {
public:
  virtual ~ModelState() {};
  virtual void write(std::ostream &os) const=0;
  virtual bool read(const std::string&)=0;
  virtual int getModelCount() const=0;
  virtual int getModelState() const=0;
  virtual int getPartitionCount() const=0;
  virtual double getValue(int i) const=0;
  virtual void broadcastToMPIWorkers(const MPIManager *mpi) {}
  virtual bool isSpinModelState() {return false;}
};
#endif
