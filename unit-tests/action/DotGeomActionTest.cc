#include <gtest/gtest.h>

#include "action/DotGeomAction.h"

#include "advancer/MultiLevelSamplerFake.h"
#include "base/Beads.h"
#include "base/SimulationInfo.h"
#include "base/Species.h"


namespace {

class DotGeomActionTest: public ::testing::Test {
protected:

    virtual void SetUp() {
        sampler = new MultiLevelSamplerFake(npart, nmoving, nslice);
        species.count = npart;
        simInfo.setTau(0.1);
        tau=0.1;
    }

    virtual void TearDown() {
        delete sampler;
    }

    MultiLevelSamplerFake *sampler;
    Species species;
    SimulationInfo simInfo;
    double tau;
    static const int npart=1;
    static const int nmoving=1;
    static const int nlevel=6;
    static const int nslice=64;

    void setIdenticalPaths() {
        Beads<NDIM>::Vec position(0.0);
        Beads<NDIM> sectionBeads = sampler->getSectionBeads();
        Beads<NDIM> movingBeads = sampler->getMovingBeads();
        for (int i = 0; i < nslice; ++i) {
            sectionBeads(0,i) = position;
            movingBeads(0,i) = position;
        }
    }
};

TEST_F(DotGeomActionTest, getActionDifferenceForIdenticalPathsIsZero) {
    DotGeomAction action(tau);
    setIdenticalPaths();
    double deltaAction = action.getActionDifference(*sampler, 0);
    ASSERT_NEAR(0.0, deltaAction, 1e-12);
}

TEST_F(DotGeomActionTest, getActionDifferenceForOneMovedBead) {
    DotGeomAction action(tau);
    setIdenticalPaths();
    Beads<NDIM> *movingBeads = sampler->movingBeads;
    Beads<NDIM>::Vec position(1.0, 2.0, -10.0);
    (*movingBeads)(0, 32) = position;
    double deltaAction = action.getActionDifference(*sampler, 0);
    ASSERT_NEAR(0, deltaAction, 1e-12);
}

}
