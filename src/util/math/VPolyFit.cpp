#include "VPolyFit.h"
#include "util/math/BLAS.h"

VPolyFit::VPolyFit(int dataCount, int dimension, const double* xdata,
        const double* ydata)
    :   xdata(xdata),
        ydata(ydata),
        dataCount(dataCount),
        dimension(dimension) {
    solution = new double[dimension];
    lastDelta = new double[dimension];
    worka = new double[dataCount * dimension];
    workc = new double[dataCount * dimension];
    workd = new double[dataCount * dimension];
}

VPolyFit::~VPolyFit() {
    delete[] solution;
    delete[] lastDelta;
    delete[] worka;
    delete[] workc;
    delete[] workd;
}

void VPolyFit::fit() {
    int totalCount = dataCount * dimension;
    const double* lasty = ydata + (dataCount - 1) * dimension;

    BLAS::dcopy(dimension, lasty, 1, solution, 1);
    BLAS::dcopy(totalCount, ydata, 1, workc, 1);
    BLAS::dcopy(totalCount, ydata, 1, workd, 1);

    for (int j = 1; j < dataCount; ++j) {
        for (int i = 0; i < dataCount - j; ++i) {
            double* ciplus1 = workc + (i + 1) * dimension;
            double* di = workd + i * dimension;
            double* ci = workc + i * dimension;

            double deltaXInverse = 1. / (xdata[i] - xdata[i + j]);

            BLAS::dcopy(dimension, ciplus1, 1, worka, 1);
            BLAS::dscal(dimension, deltaXInverse, worka, 1);
            BLAS::daxpy(dimension, -deltaXInverse, di, 1, worka, 1);

            BLAS::dcopy(dimension, worka, 1, ci, 1);
            BLAS::dscal(dimension, xdata[i], ci, 1);
            BLAS::dcopy(dimension, worka, 1, di, 1);
            BLAS::dscal(dimension, xdata[i + j], di, 1);
        }
        double* lastd = workd + (dataCount - j - 1) * dimension;
        BLAS::daxpy(dimension, 1.0, lastd, 1, solution, 1);
    }
    BLAS::dcopy(dimension, workc, 1, lastDelta, 1);
}

const double* VPolyFit::getSolution() const {
    return solution;
}

const double* VPolyFit::getLastDelta() const {
    return lastDelta;
}


