#include "StdoutScalarReportWriter.h"
#include "stats/ScalarEstimator.h"
#include "stats/SimpleScalarAccumulator.h"

StdoutScalarReportWriter::StdoutScalarReportWriter(int stepCount) {
    nstep = stepCount;
}

StdoutScalarReportWriter::~StdoutScalarReportWriter() {
}

void StdoutScalarReportWriter::startReport(const ScalarEstimator *est,
        const SimpleScalarAccumulator *acc) {
    sum.resize(sum.size() + 1);
    sum2.resize(sum2.size() + 1);
    norm.resize(norm.size() + 1);
    istep = 0;
    sum = 0;
    sum2 = 0;
    norm = 0;
}

void StdoutScalarReportWriter::reportStep(const ScalarEstimator *est,
        const SimpleScalarAccumulator *acc) {
    double value = est->getValue();
    if (acc) {
        value = (acc->getValue() + est->getShift()) * est->getScale();
    }
    sum(iscalar) += value;
    sum2(iscalar) += value * value;
    norm(iscalar) += 1;
    std::cout << est->getName();
    if (est->getUnitName() != "")
        std::cout << " (" << est->getUnitName() << ")";
    std::cout << ": " << value << ", " << "Av="
            << sum(iscalar) / (norm(iscalar)) << " +-"
            << sqrt(
                    sum2(iscalar)
                            - sum(iscalar) * sum(iscalar) / (norm(iscalar)))
                    / (norm(iscalar) - 1) << std::endl;
    ++iscalar;
}

void StdoutScalarReportWriter::startBlock(int istep) {
    this->istep = istep;
    iscalar = 0;
}
