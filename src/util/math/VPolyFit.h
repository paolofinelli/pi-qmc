#ifndef VPOLYFIT_H_
#define VPOLYFIT_H_

class VPolyFit {
public:
    VPolyFit(int dataCount, int dimension,
            const double* xdata, const double* ydata);
    virtual ~VPolyFit();

    void fit();
    const double* getSolution() const;
    const double* getLastDelta() const;
private:
    const double* xdata;
    const double* ydata;
    double* solution;
    double* lastDelta;
    double* worka;
    double* workc;
    double* workd;
    int dataCount;
    int dimension;
};


#endif
