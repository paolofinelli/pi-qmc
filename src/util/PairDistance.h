#ifndef __PairDistance_h_
#define __PairDistance_h_
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include "SuperCell.h"
#include <cstdlib>
#include <blitz/array.h>
#include <blitz/tinyvec-et.h>

typedef blitz::TinyVector<double, NDIM> Vec;
/// Base class for distance functions.
class PairDistance {
public:
    virtual ~PairDistance() {}
    virtual double operator()(const Vec &r1, const Vec &r2,
            const SuperCell &cell) const=0;

};

/// Distance taken from radial separation.
class PairRadial: public PairDistance {
public:
    PairRadial(int idir = -1) :
            mask(1.0) {
        if (idir != -1)
            mask(idir) = 0;
    }
    virtual double operator()(const Vec &r1, const Vec &r2,
            const SuperCell &cell) const {
        Vec delta = r1 - r2;
        cell.pbc(delta);
        double radius2 = 0;
        for (int i = 0; i < NDIM; ++i)
            radius2 += delta(i) * delta(i) * mask(i);
        return sqrt(radius2);
    }
    Vec mask;
};
/// Distance taken from radius of particle 1.
class PairRadial1: public PairDistance {
public:
    PairRadial1(int idir = -1) :
            mask(1.0) {
        if (idir != -1)
            mask(idir) = 0;
    }
    virtual double operator()(const Vec &r1, const Vec &r2,
            const SuperCell &cell) const {
        double radius2 = 0;
        for (int i = 0; i < NDIM; ++i)
            radius2 += r1(i) * r1(i) * mask(i);
        return sqrt(radius2);
    }
    Vec mask;
};
/// Distance taken from radius of particle 2.
class PairRadial2: public PairDistance {
public:
    PairRadial2(int idir = -1) :
            mask(1.0) {
        if (idir != -1)
            mask(idir) = 0;
    }
    virtual double operator()(const Vec &r1, const Vec &r2,
            const SuperCell &cell) const {
        double radius2 = 0;
        for (int i = 0; i < NDIM; ++i)
            radius2 += r2(i) * r2(i) * mask(i);
        return sqrt(radius2);
    }
    Vec mask;
};
/// Distance taken from cartesian position of particle 1.
class PairCart1: public PairDistance {
public:
    PairCart1(int idim) :
            idim(idim) {
    }
    ;
    int idim;
    virtual double operator()(const Vec &r1, const Vec &r2,
            const SuperCell &cell) const {
        return r1[idim];
    }
    ;
};
/// Distance taken from cartesian position of particle 2.
class PairCart2: public PairDistance {
public:
    PairCart2(int idim) :
            idim(idim) {
    }
    ;
    int idim;
    virtual double operator()(const Vec &r1, const Vec &r2,
            const SuperCell &cell) const {
        return r2[idim];
    }
    ;
};
/// Distance taken from cartesian separation.
class PairCart: public PairDistance {
public:
    PairCart(int idim) :
            idim(idim) {
    }
    ;
    int idim;
    virtual double operator()(const Vec &r1, const Vec &r2,
            const SuperCell &cell) const {
        Vec delta = r1 - r2;
        cell.pbc(delta);
        return delta[idim];
    }
};
/// Angle between particles (idim and jdim specify plane).
class PairPlaneAngle: public PairDistance {
public:
    PairPlaneAngle(int idim, int jdim) :
            idim(idim), jdim(jdim) {
    }
    ;
    int idim, jdim;
    static const double PI;
    virtual double operator()(const Vec &r1, const Vec &r2,
            const SuperCell &cell) const {
        double angle1 = atan2(r1(idim), r1(jdim));
        double angle2 = atan2(r2(idim), r2(jdim));
        double angle = angle1 - angle2;
        return (angle > PI) ? angle - 2 * PI :
               (angle > -PI) ? angle : angle + 2 * PI;
    }
};
/// Angle between particles.
class PairAngle: public PairDistance {
public:
    virtual double operator()(const Vec &r1, const Vec &r2,
            const SuperCell &cell) const {
        return acos(dot(r1, r2) / sqrt(dot(r1, r1) * dot(r2, r2) + 1e-100));
    }
};
/// Angle of particle 1 (idim and jdim specify plane).
class PairAngle1: public PairDistance {
public:
    PairAngle1(int idim, int jdim) :
            idim(idim), jdim(jdim) {
    }
    ;
    int idim, jdim;
    virtual double operator()(const Vec &r1, const Vec &r2,
            const SuperCell &cell) const {
        return atan2(r1(idim), r1(jdim));
    }
};
/// Angle of particle 2 (idim and jdim specify plane).
class PairAngle2: public PairDistance {
public:
    PairAngle2(int idim, int jdim) :
            idim(idim), jdim(jdim) {
    }
    ;
    int idim, jdim;
    virtual double operator()(const Vec &r1, const Vec &r2,
            const SuperCell &cell) const {
        return atan2(r2(idim), r2(jdim));
    }
};
#endif
