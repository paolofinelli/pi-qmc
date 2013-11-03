#include "config.h"
#ifdef ENABLE_MPI
#include <mpi.h>
#endif
#include "WritePaths.h"
#include "base/Beads.h"
#include "base/BeadFactory.h"
#include "base/ModelState.h"
#include "base/Paths.h"
#include "base/SimulationInfo.h"
#include "stats/MPIManager.h"
#include <iostream>
#include <sstream>
#include <fstream>

WritePaths::WritePaths(Paths& paths, const std::string& filename, int dumpFreq,
  int maxConfigs,  bool writeMovie, const SimulationInfo& simInfo, 
  MPIManager *mpi, const BeadFactory& beadFactory)
  : filename(filename), paths(paths), mpi(mpi), beadFactory(beadFactory), 
    dumpFreq(dumpFreq), simInfo(simInfo), maxConfigs(maxConfigs),
    writeMovie(writeMovie) {
  if (writeMovie) movieFile = new std::ofstream("pathMovie", std::ios::out);
}

void WritePaths::run() {
  //Write paths (movie) every dumpFreq (dumpMovieCounter) times
  static int dumpPathCounter=0;
  if (dumpPathCounter < dumpFreq) {
    dumpPathCounter++;
    return;
  }
  dumpPathCounter=0;
  static int dumpMovieCounter=0;
  dumpMovieCounter++;

  //Prepare file handlers 
  int workerID=(mpi)?mpi->getWorkerID():0;
  int nclone=(mpi)?mpi->getNClone():1;
  int cloneID=(mpi)?mpi->getCloneID():0;

   
  std::stringstream ext;
  if (nclone>1) ext << cloneID;
  std::ofstream *file=0;   
  //paths files
  Permutation perm(paths.getGlobalPermutation());   
  if (workerID==0){
    file = new std::ofstream((filename+ext.str()).c_str());
    *file <<"#Permutations ";
    for (int i=0; i<paths.getNPart(); i++) *file << perm[i]<< " ";
    *file << std::endl;
    *file << "#Path coordinates: " << paths.getNPart()
	  << " particles in " << NDIM << "-d";
    if (paths.hasModelState()) {
      paths.getModelState()->write(*file);
    }
    *file << std::endl;
    file->precision(15);
  }
  //movie files
  if (cloneID==0 && workerID==0 && writeMovie) {
    if (dumpMovieCounter > maxConfigs) {
      movieFile->close();
      movieFile->open("pathMovie", std::ios::trunc);
      movieFile->precision(15);
      dumpMovieCounter=0;
    }
    /*  
     *movieFile <<"#Permutations ";
     for (int i=0; i<paths.getNPart(); i++) *movieFile << perm[i]<< " ";
     *movieFile << std::endl;
     *movieFile << "# " << paths.getNPart()<<"   ";
     for (int ispec=0;ispec<simInfo.getNSpecies(); ispec++) *movieFile <<simInfo.getSpecies(ispec).count<<"  ";
     *movieFile <<std::endl;
     */
  }

  /// Now determine how many chunked steps we need to write out all slices.
  int nslice=paths.getNSlice();
  int ifirst=paths.getLowestOwnedSlice(false);
  int imax=paths.getHighestOwnedSlice(false);
#ifdef ENABLE_MPI
  int nworker=(mpi)?mpi->getNWorker():1;
  if (mpi && nworker>1) {
    mpi->getWorkerComm().Bcast(&nslice,1,MPI::INT,0);
    mpi->getWorkerComm().Bcast(&ifirst,1,MPI::INT,0);
    mpi->getWorkerComm().Bcast(&imax,1,MPI::INT,0);
  }
#endif

  int nslicePerChunk = imax-ifirst-2;


  /// Loop over chunks.
  for (int ichunk=0; ichunk<(int)ceil(nslice*1./nslicePerChunk); ++ichunk) {
    int lastSlice=(nslice+ifirst)-ichunk*nslicePerChunk;
    if (lastSlice>nslicePerChunk) lastSlice=nslicePerChunk;
    if (workerID==0){
      for (int islice=0; islice<lastSlice; ++islice) {
        for (int ipart=0; ipart<paths.getNPart(); ++ipart) {
          Paths::Vec p = paths(ipart,islice);
          for (int i=0; i<NDIM; ++i) *file << p[i] << " ";
          if (writeMovie)  {
            for (int i=0; i<NDIM; ++i) *movieFile << p[i] << " ";
          }
        }
        *file << std::endl;
        if (writeMovie) *movieFile << std::endl;
      }
    }
    paths.shift(lastSlice);
  }
  if (workerID==0) delete file;
}
