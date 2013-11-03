#include <gtest/gtest.h>
#include "util/propagator/Propagator.h"
#include "util/propagator/PropagatorGrid.h"
#include <cmath>

namespace {

class PropagatorTest: public testing::Test {
protected:
    void SetUp() {
        mass = 1.0;
        omega = 1.0;
        tau = 0.124235;
        x0 = 1.0;
    }

    /// Approximate grid propagator with continuum propagator.
    double approximateK0(double x1, double x2, double tau, double gridDeltaX) {
        const double PI = 3.141592653589793;
        double delta2 = (x1 - x2) * (x1 -x2);
        double prefactor = gridDeltaX / sqrt(2 * PI * tau / mass);
        return prefactor * exp(-0.5 * mass * delta2 / tau);
    }


    double K(double x1, double x2, double tau, double gridDeltaX) {
        const double PI = 3.141592653589793;
        double sinhwt = sinh(omega * tau);
        double coshwt = cosh(omega * tau);
        return gridDeltaX
                * sqrt(mass * omega / (2.0 * PI * sinhwt))
                * exp(-(mass*omega*(x1*x1 + x2*x2)*coshwt - 2* x1*x2)
                        / (2.0 * sinhwt));
    }

    double mass;
    double omega;
    double tau;
    double x0;
};

TEST_F(PropagatorTest, TestKineticEvolution) {
    Propagator propagator(mass, tau, x0);
    propagator.setPotential(Propagator::zeroPotential);
    double value = propagator.evaluate();
    double deltaX = propagator.getGridSpacing();
    double expect = approximateK0(x0, x0, tau, deltaX);
    ASSERT_NEAR(expect, value, 1e-12);
}

TEST_F(PropagatorTest, TestSHOEvolution) {
    Propagator propagator(mass, tau, x0);
    propagator.setPotential(Propagator::harmonicPotential);
    double value = propagator.evaluate();
    double deltaX = propagator.getGridSpacing();
    double expect = K(x0, x0, tau, deltaX);
    ASSERT_NEAR(expect, value, 1e-9);
}

}
