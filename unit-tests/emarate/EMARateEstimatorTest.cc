#include <gtest/gtest.h>

#include "emarate/EMARateEstimator.h"
#include "action/coulomb/CoulombLinkAction.h"
#include "base/BeadFactory.h"
#include "base/SerialPaths.h"
#include "base/SimulationInfo.h"
#include "util/SuperCell.h"

namespace {

class EMARateEstimatorTest: public testing::Test {
protected:

    virtual void SetUp() {
        separation = 10.0;
        simInfo.nslice = 128;
        simInfo.npart = 2;
        simInfo.tau = 0.1;
        SuperCell::Vec length = 100.0;
        simInfo.superCell = new SuperCell(length);

        species1 = new Species("h", 1, 1.0, 1.0, 1, false);
        species1->ifirst = 0;
        species2 = new Species("e", 1, 1.0, -1.0, 1, false);
        species2->ifirst = 1;

        paths = new SerialPaths(simInfo.npart, simInfo.nslice, simInfo.tau,
                *simInfo.superCell, beadFactory);
        coefficient = 1.0;
    }

    virtual void TearDown() {
        delete species1;
        delete species2;
        delete estimator;
        delete paths;
    }

    void createEstimator() {
        estimator = new EMARateEstimator(simInfo, species1, species2, coefficient);
        estimator->reset();
    }

    void setDirectPaths() {
        Paths::Vec holePosition = Paths::Vec(0.0, 0.0, 0.0);
        Paths::Vec electronPosition = Paths::Vec(separation, separation, separation);
        for (int sliceIndex = 0; sliceIndex < simInfo.nslice; ++sliceIndex) {
            (*paths)(0, sliceIndex) = holePosition;
            (*paths)(1, sliceIndex) = electronPosition;
        }
    }

    void setExchangingPaths() {
        Paths::Vec afterPosition = Paths::Vec(0.0, 0.0, 0.0);
        Paths::Vec beforePosition = Paths::Vec(separation, separation, separation);
        (*paths)(0,0) = beforePosition;
        (*paths)(1,0) = afterPosition;

        double inverseSliceCount = 1.0 / paths->getNSlice();
        for (int sliceIndex = 1; sliceIndex < simInfo.nslice; ++sliceIndex) {
            double x = sliceIndex * inverseSliceCount;
            Paths::Vec position = (1.0 - x) * afterPosition + x * beforePosition;
            (*paths)(0, sliceIndex) = position;
            (*paths)(1, sliceIndex) = position;
        }
    }

    EMARateEstimator *estimator;
    Species* species1;
    Species* species2;
    double coefficient;
    SimulationInfo simInfo;
    SerialPaths *paths;
    BeadFactory beadFactory;
    double separation;
};



TEST_F(EMARateEstimatorTest, testDirectPaths) {
    createEstimator();
    setDirectPaths();
    estimator->evaluate(*paths);
    double value = estimator->calcValue();
    double expect = 1.0;
    ASSERT_DOUBLE_EQ(expect, value);
}

TEST_F(EMARateEstimatorTest, testExchangingPaths) {
    createEstimator();
    setExchangingPaths();
    estimator->evaluate(*paths);
    double value = estimator->calcValue();
    double expect = 0.0;
    ASSERT_DOUBLE_EQ(expect, value);
}

TEST_F(EMARateEstimatorTest, testNearbyDirectPaths) {
    createEstimator();
    separation = 0.1;
    setDirectPaths();
    estimator->evaluate(*paths);
    double value = estimator->calcValue();
    double delta2 = NDIM * separation * separation;
    double actionDifference = 2 * 0.5 * delta2 / simInfo.tau;
    double expect = 1.0 / (1.0 + exp(-actionDifference));
    ASSERT_DOUBLE_EQ(expect, value);
}

TEST_F(EMARateEstimatorTest, testLargeCoefficentValue) {
    coefficient = 10.0;
    createEstimator();
    separation = 0.1;
    setDirectPaths();
    estimator->evaluate(*paths);
    double value = estimator->calcValue();
    double delta2 = NDIM * separation * separation;
    double actionDifference = 2 * 0.5 * delta2 / simInfo.tau;
    double expect = 1.0 / (1.0 + coefficient * exp(-actionDifference));
    ASSERT_DOUBLE_EQ(expect, value);
}

TEST_F(EMARateEstimatorTest, testSmallCoefficentValue) {
    separation = 0.1;
    coefficient = 0.1;
    createEstimator();
    setDirectPaths();
    estimator->evaluate(*paths);
    double value = estimator->calcValue();
    double delta2 = NDIM * separation * separation;
    double actionDifference = 2 * 0.5 * delta2 / simInfo.tau;
    double expect = 1.0 / (1.0 + coefficient * exp(-actionDifference));
    ASSERT_DOUBLE_EQ(expect, value);
}

TEST_F(EMARateEstimatorTest, testCoulombInteraction) {
    createEstimator();
    double epsilon = 1.0;
    int norder = 1;
    estimator->includeCoulombContribution(epsilon, norder);
    separation = 0.1;
    setDirectPaths();
    estimator->evaluate(*paths);
    double value = estimator->calcValue();

    double q1q2 = -1.0;
    double mu = 0.5;
    CoulombLinkAction coulomb(q1q2, epsilon, mu, simInfo.tau, norder);
    CoulombLinkAction::Vec zero = 0.0;
    CoulombLinkAction::Vec delta = separation;

    double delta2 = NDIM * separation * separation;
    double actionDifference = 2 * 0.5 * delta2 / simInfo.tau;
    actionDifference += 2.0 * coulomb.getValue(zero, delta);
    actionDifference -= 2.0 * coulomb.getValue(delta, delta);
    double expect = 1.0 / (1.0 + exp(-actionDifference));
    ASSERT_DOUBLE_EQ(expect, value);
}

}
