#include "config.h"
#ifdef ENABLE_MPI
#include <mpi.h>
#endif
#include "ScalarEstimator.h"
#include "MPIManager.h"
#include "ReportWriters.h"
#include "ScalarAccumulator.h"

ScalarEstimator::ScalarEstimator(const std::string& name)
    :   Estimator(name,"","scalar"),
        accumulator(0),
        scale(1.),
        shift(0.) {
}

ScalarEstimator::ScalarEstimator(const std::string &name,
        const std::string &typeString, const std::string &unitName,
        double scale, double shift)
    :   Estimator(name,typeString,unitName),
        accumulator(0),
        scale(scale),
        shift(shift) {
}

ScalarEstimator::~ScalarEstimator() {
    delete accumulator;
}

double ScalarEstimator::getValue() const {
    return scale * (value + shift);
}

void ScalarEstimator::setValue(const double v) {
    value = v;
}

void ScalarEstimator::averageOverClones(const MPIManager* mpi) {
    if (accumulator) {
        accumulator->calculateTotal();
        accumulator->reset();
        accumulator->averageOverClones();
    } else {
        double v = calcValue();
        value = v;
        reset();
#ifdef ENABLE_MPI
        if (mpi->isCloneMain()) {
            mpi->getCloneComm().Reduce(&v,&value,1,MPI::DOUBLE,MPI::SUM,0);
            int size = mpi->getNClone();
            setValue(value/size);
        }
#endif
    }
}

void ScalarEstimator::startReport(ReportWriters *writers) {
    if (accumulator) {
        accumulator->startReport(writers, this);
    } else {
        SimpleScalarAccumulator *accumulator = 0;
        writers->startScalarReport(this, accumulator);
    }
}

void ScalarEstimator::reportStep(ReportWriters *writers) {
    if (accumulator) {
        accumulator->reportStep(writers, this);
    } else {
        SimpleScalarAccumulator *accumulator = 0;
        writers->reportScalarStep(this, accumulator);
    }
}

const double ScalarEstimator::getScale() const {
    return scale;
}

const double ScalarEstimator::getShift() const {
    return shift;
}
