#include <gtest/gtest.h>
#include "advancer/CollectiveSectionMover.h"
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <blitz/tinyvec-et.h>
#include <blitz/tinymat.h>
#include "util/SuperCell.h"

typedef blitz::TinyVector<double, NDIM> Vec;
typedef blitz::TinyMatrix<double, NDIM, NDIM> Mat;

bool vecEquals(const Vec& r1, const Vec& r2) {
    return dot(r1 - r2, r1 - r2) < 1e-10;
}

namespace {

class CollectiveSectionMoverTest: public ::testing::Test {
protected:

    virtual void SetUp() {
        radius = 1.0;
        amplitude = Vec(0.4, 0.0, 0.0);
        center = Vec(4.9, 4.0, 4.5);
        sliceCount = 9;
        length = 10.0;
        cell = new SuperCell(Vec(length, length, length));
        cell->computeRecipricalVectors();
        int npart = 0;
        min = Vec(-5.0, -5.0, -5.0);
        max = Vec(5.0, 5.0, 5.0);
        mover = new CollectiveSectionMover(radius, amplitude, min, max, cell);
        mover->setRadius(radius);
        mover->setAmplitude(amplitude);
        mover->setSliceCount(sliceCount);
        mover->setCenter(center);
    }

    virtual void TearDown() {
        delete cell;
        delete mover;
    }

    void ASSERT_VEC_EQ(const Vec& v1, const Vec& v2,
            double epsilon = 1e-14) const {
        ASSERT_NEAR(v1(0), v2(0), epsilon);
        ASSERT_NEAR(v1(1), v2(1), epsilon);
        ASSERT_NEAR(v1(2), v2(2), epsilon);
    }

    void ASSERT_MAT_EQ(const Mat& mat1, const Mat& mat2,
            double epsilon = 1e-14) const {
        ASSERT_NEAR(mat1(0,0), mat2(0,0), epsilon);
        ASSERT_NEAR(mat1(0,1), mat2(0,1), epsilon);
        ASSERT_NEAR(mat1(0,2), mat2(0,2), epsilon);
        ASSERT_NEAR(mat1(1,0), mat2(1,0), epsilon);
        ASSERT_NEAR(mat1(1,1), mat2(1,1), epsilon);
        ASSERT_NEAR(mat1(1,2), mat2(1,2), epsilon);
        ASSERT_NEAR(mat1(2,0), mat2(2,0), epsilon);
        ASSERT_NEAR(mat1(2,1), mat2(2,1), epsilon);
        ASSERT_NEAR(mat1(2,2), mat2(2,2), epsilon);
    }

    double radius;
    Vec amplitude;
    Vec center;
    Vec min;
    Vec max;
    int sliceCount;
    CollectiveSectionMover *mover;
    double length;
    SuperCell *cell;

    static Mat matFromData(double *data) {
        Mat matrix;
        for (int i = 0; i < 9; ++i) {
            matrix.data()[i] = data[i];
        }
        return matrix;
    }
};

TEST_F(CollectiveSectionMoverTest, testValueOfNslice) {
    ASSERT_EQ(9, mover->getSliceCount());
}

TEST_F(CollectiveSectionMoverTest, testMoveAtCenter) {
    Vec oldr = center;
    int sliceIndex = 4;
    Vec newr = mover->calcShift(oldr, sliceIndex);
    Vec expect = (amplitude - Vec(length, 0, 0)) + center;
    ASSERT_VEC_EQ(expect, newr);
}

TEST_F(CollectiveSectionMoverTest, DISABLED_testMoveAtCenterWithNoAmplitude) {
    amplitude = 0.0;
    mover->setAmplitude(amplitude);
    Vec oldr = center;
    int sliceIndex = 4;
    Vec newr = mover->calcShift(oldr, sliceIndex);
    ASSERT_VEC_EQ(center, newr);
}

TEST_F(CollectiveSectionMoverTest, testMoveAwayFromCenter) {
    Vec oldr = Vec(0.0, 0.5, 0.1) + center;
    int sliceIndex = 4;
    Vec newr = mover->calcShift(oldr, sliceIndex);
    Vec expect = 0.74 * amplitude - Vec(length, 0, 0) + oldr;
    ASSERT_VEC_EQ(expect, newr);
}

TEST_F(CollectiveSectionMoverTest, testDoesNotMoveOutsideOfRadius) {
    Vec oldr = Vec(0.0, -1.5, 0.1) + center;
    int sliceIndex = 4;
    Vec newr = mover->calcShift(oldr, sliceIndex);
    ASSERT_VEC_EQ(oldr, newr);
}

TEST_F(CollectiveSectionMoverTest, testDoesNotMoveAtFirstSlice) {
    Vec oldr = Vec(0.0, 0.5, 0.1) + center;
    int sliceIndex = 0;
    Vec newr = mover->calcShift(oldr, sliceIndex);
    ASSERT_VEC_EQ(oldr, newr);
}

TEST_F(CollectiveSectionMoverTest, testMoveAwayFromCenterSlice) {
    Vec oldr = Vec(0.0, 0.5, 0.1) + center;
    int sliceIndex = 2;
    Vec newr = mover->calcShift(oldr, sliceIndex);
    Vec expect = 0.555 * amplitude - Vec(length, 0, 0) + oldr;
    ASSERT_VEC_EQ(expect, newr);
}

TEST_F(CollectiveSectionMoverTest, testJacobianMatrixAtCenter) {
    Vec oldr = center;
    int sliceIndex = 4;
    Mat jacobian = mover->calcJacobian(oldr, sliceIndex);
    double data[9] = { 1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0 };
    Mat expect = matFromData(data);
    ASSERT_MAT_EQ(expect, jacobian);
}

TEST_F(CollectiveSectionMoverTest, testJacobianAwayFromCenter) {
    Vec oldr = Vec(0.0, 0.5, 0.1) + center;
    int sliceIndex = 4;
    Mat jacobian = mover->calcJacobian(oldr, sliceIndex);
    double data[9] = { 1.0, -0.4, -0.08, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0 };
    Mat expect = matFromData(data);
    ASSERT_MAT_EQ(expect, jacobian);
}

TEST_F(CollectiveSectionMoverTest, testJacobianAwayFromCenterSlice) {
    Vec oldr = Vec(0.0, 0.5, 0.1) + center;
    int sliceIndex = 2;
    Mat jacobian = mover->calcJacobian(oldr, sliceIndex);
    double data[9] = { 1.0, -0.3, -0.06, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0 };
    Mat expect = matFromData(data);
    ASSERT_MAT_EQ(expect, jacobian);
}

TEST_F(CollectiveSectionMoverTest, testJacobianOutsideOfRadius) {
    Vec oldr = Vec(0.0, 1.5, 0.1) + center;
    int sliceIndex = 2;
    Mat jacobian = mover->calcJacobian(oldr, sliceIndex);
    double data[9] = { 1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0 };
    Mat expect = matFromData(data);
    ASSERT_MAT_EQ(expect, jacobian);
}

TEST_F(CollectiveSectionMoverTest, testReverseMove) {
    Vec oldr = Vec(0.05, 0.3, -0.2) + center;
    int sliceIndex = 3;
    Vec newr = mover->calcShift(oldr, sliceIndex);
    Vec backr = mover->calcInverseShift(newr, sliceIndex);
    ASSERT_VEC_EQ(backr, oldr, 1e-12);
}

}

