#pragma once

#include <CCGeom.h>
#include <PointProjectionTools.h>
#include <SquareMatrix.h>

#include <QMetaType>
#include <cmath>
#include <numbers>

// Rigid transformation as ZYX Euler angles (degrees) + translation.
// Convention: R = Rz(alpha) * Ry(beta) * Rx(gamma)
struct Transformation3D {
    double alpha = 0.0;  // degrees, rotation around Z
    double beta  = 0.0;  // degrees, rotation around Y
    double gamma = 0.0;  // degrees, rotation around X
    double tx    = 0.0;
    double ty    = 0.0;
    double tz    = 0.0;

    static Transformation3D fromCCTransform(
        const CCCoreLib::PointProjectionTools::Transformation& t)
    {
        constexpr double toDeg = 180.0 / std::numbers::pi;
        Transformation3D r;
        const auto& R = t.R;
        // R[2][0] = -sin(beta)
        double sb = -R.getValue(2, 0);
        sb = std::clamp(sb, -1.0, 1.0);
        r.beta = std::asin(sb) * toDeg;
        const double cbeta = std::cos(r.beta * std::numbers::pi / 180.0);
        if (std::abs(cbeta) > 1e-10) {
            r.alpha = std::atan2(R.getValue(1, 0), R.getValue(0, 0)) * toDeg;
            r.gamma = std::atan2(R.getValue(2, 1), R.getValue(2, 2)) * toDeg;
        } else {
            // Gimbal lock: alpha absorbs the rotation
            r.alpha = std::atan2(-R.getValue(0, 1), R.getValue(1, 1)) * toDeg;
            r.gamma = 0.0;
        }
        r.tx = t.T.x;
        r.ty = t.T.y;
        r.tz = t.T.z;
        return r;
    }

    bool operator==(const Transformation3D&) const = default;
    CCCoreLib::PointProjectionTools::Transformation toCCTransform() const {
        constexpr double toRad = std::numbers::pi / 180.0;
        const double ca = std::cos(alpha * toRad), sa = std::sin(alpha * toRad);
        const double cb = std::cos(beta  * toRad), sb = std::sin(beta  * toRad);
        const double cg = std::cos(gamma * toRad), sg = std::sin(gamma * toRad);

        CCCoreLib::PointProjectionTools::Transformation t;
        t.s = 1.0;
        t.R = CCCoreLib::SquareMatrixd(3);
        // R = Rz(alpha) * Ry(beta) * Rx(gamma)
        t.R.setValue(0, 0, ca*cb);             t.R.setValue(0, 1, ca*sb*sg - sa*cg); t.R.setValue(0, 2, ca*sb*cg + sa*sg);
        t.R.setValue(1, 0, sa*cb);             t.R.setValue(1, 1, sa*sb*sg + ca*cg); t.R.setValue(1, 2, sa*sb*cg - ca*sg);
        t.R.setValue(2, 0, -sb);               t.R.setValue(2, 1, cb*sg);            t.R.setValue(2, 2, cb*cg);
        t.T = CCVector3d(tx, ty, tz);
        return t;
    }
};

Q_DECLARE_METATYPE(Transformation3D)
