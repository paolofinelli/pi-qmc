#include "config.h"
#ifdef ENABLE_MPI
#include <mpi.h>
#endif
#include "EstimatorManager.h"
#include "MPIManager.h"
#include "EstimatorIterator.h"
#include "ascii/AsciiReportBuilder.h"
#include "hdf5/H5ReportBuilder.h"
#include "stdout/StdoutReportBuilder.h"
#include "PartitionedScalarAccumulator.h"
#include "SimpleScalarAccumulator.h"
#include "PartitionWeight.h"
#include <algorithm>
#include <functional>
#include <fstream>

EstimatorManager::EstimatorManager(const std::string& filename, MPIManager *mpi,
        const SimInfoWriter *simInfoWriter) :
    filename(filename),
    mpi(mpi),
    simInfoWriter(simInfoWriter),
    partitionWeight(0),
    isSplitOverStates(false) {
}

EstimatorManager::~EstimatorManager() {
    delete simInfoWriter;
    for (EstimatorIter i = estimator.begin(); i != estimator.end(); ++i)
        delete *i;
    for (BuilderIter i = builders.begin(); i != builders.end(); ++i)
        delete *i;
}

void EstimatorManager::add(Estimator* e) {
    estimator.push_back(e);
}

PartitionWeight* EstimatorManager::getPartitionWeight() const {
    return partitionWeight;
}

void EstimatorManager::setInputFilename(const std::string& inputFilename) {
    this->inputFilename = inputFilename;
}

void EstimatorManager::createBuilders(const std::string& filename,
        const SimInfoWriter* simInfoWriter) {
    if (!mpi || mpi->isMain()) {
        builders.push_back(new H5ReportBuilder(filename, simInfoWriter));
        builders.push_back(new StdoutReportBuilder());
        builders.push_back(new AsciiReportBuilder("pimc.dat"));
    }
    recordInputDocument(inputFilename);
}

void EstimatorManager::startWritingGroup(const int nstep,
        const std::string& name) {
    createBuilders(filename, simInfoWriter);
    this->nstep = nstep;
    istep = 0;
    if (!mpi || mpi->isMain()) {
        for (BuilderIter builder = builders.begin(); builder != builders.end();
                ++builder) {
            (*builder)->initializeReport(this);
        }
    }
}

void EstimatorManager::writeStep() {
    // Calculate averages over clones.
    for (EstimatorIter est = estimator.begin(); est != estimator.end(); ++est) {
        (*est)->averageOverClones(mpi);
    };
    // Write step data to using EstimatorReportBuilder objects.
    if (!mpi || mpi->isMain()) {
        for (BuilderIter builder = builders.begin(); builder != builders.end();
                ++builder) {
            (*builder)->collectAndWriteDataBlock(this);
        }
    }
    // Avoid a race condition.
#ifdef ENABLE_MPI
    //char buffer[256];
    //int namelength;
    //MPI::Get_processor_name (buffer, namelength);
    //std::string name(buffer, namelength);
    //std::time_t rawtime;
    //std::time(&rawtime);
    //std::cout << "Barrier reached by " << name << " at "
    //          << ctime(&rawtime) << std::endl;
    if (mpi) {
        mpi->getWorkerComm().Barrier();
        if (mpi->isCloneMain()) {
            mpi->getCloneComm().Barrier();
        }
    }
#endif
}

std::vector<Estimator*>&
EstimatorManager::getEstimatorSet(const std::string& name) {
    std::vector<Estimator*>& set(estimatorSet[name]);
    if (name == "all" && set.size() != estimator.size()) {
        set.resize(estimator.size());
        std::list<Estimator*>::iterator iter = estimator.begin();
        for (unsigned int i = 0; i < set.size(); ++i)
            set[i] = *iter++;
    }
    return set;
}

void EstimatorManager::recordInputDocument(const std::string &filename) {
    if (!mpi || mpi->isMain()) {
        std::string buffer;
        std::string docstring;
        std::cout << "recording " << filename << std::endl;
        std::ifstream in(filename.c_str());
        while (!std::getline(in, buffer).eof()) {
            docstring += buffer += "\n";
        }
        for (BuilderIter builder = builders.begin(); builder != builders.end();
                ++builder) {
            (*builder)->recordInputDocument(docstring);
        }
    }
}

EstimatorIterator EstimatorManager::getEstimatorIterator() {
    return EstimatorIterator(estimator);
}

int EstimatorManager::getNStep() const {
    return nstep;
}

void EstimatorManager::setPartitionWeight(PartitionWeight *partitionWeight) {
    this->partitionWeight = partitionWeight;
}

void EstimatorManager::setIsSplitOverStates(bool isSplitOverStates) {
    this->isSplitOverStates = isSplitOverStates;
}

bool EstimatorManager::getIsSplitOverStates() {
    return isSplitOverStates;
}

ScalarAccumulator* EstimatorManager::createScalarAccumulator() {
    ScalarAccumulator *accumulator = 0;
    if (isSplitOverStates) {
        accumulator = new PartitionedScalarAccumulator(mpi, partitionWeight);
    } else {
        accumulator = new SimpleScalarAccumulator(mpi);
    }
    return accumulator;
}

