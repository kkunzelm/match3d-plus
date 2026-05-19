/*
 * Match3D+ - Dental surface comparison software
 * Copyright (C) 2026 Karl-Heinz Kunzelmann
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

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
    // Numerical constants
    static constexpr double kGimbalLockTol = 1e-10;  // Threshold for detecting gimbal lock (cos(beta) ≈ 0)
    static constexpr double kDegToRad = std::numbers::pi / 180.0;
    static constexpr double kRadToDeg = 180.0 / std::numbers::pi;

    double alpha = 0.0;  // degrees, rotation around Z
    double beta  = 0.0;  // degrees, rotation around Y
    double gamma = 0.0;  // degrees, rotation around X
    double tx    = 0.0;
    double ty    = 0.0;
    double tz    = 0.0;

    static Transformation3D fromCCTransform(
        const CCCoreLib::PointProjectionTools::Transformation& t)
    {
        Transformation3D r;
        const auto& R = t.R;
        // R[2][0] = -sin(beta)
        double sb = -R.getValue(2, 0);
        sb = std::clamp(sb, -1.0, 1.0);
        r.beta = std::asin(sb) * kRadToDeg;
        const double cbeta = std::cos(r.beta * kDegToRad);
        if (std::abs(cbeta) > kGimbalLockTol) {
            r.alpha = std::atan2(R.getValue(1, 0), R.getValue(0, 0)) * kRadToDeg;
            r.gamma = std::atan2(R.getValue(2, 1), R.getValue(2, 2)) * kRadToDeg;
        } else {
            // Gimbal lock: beta ≈ ±90°, alpha absorbs the rotation
            r.alpha = std::atan2(-R.getValue(0, 1), R.getValue(1, 1)) * kRadToDeg;
            r.gamma = 0.0;
        }
        r.tx = t.T.x;
        r.ty = t.T.y;
        r.tz = t.T.z;
        return r;
    }

    bool operator==(const Transformation3D&) const = default;
    CCCoreLib::PointProjectionTools::Transformation toCCTransform() const {
        const double ca = std::cos(alpha * kDegToRad), sa = std::sin(alpha * kDegToRad);
        const double cb = std::cos(beta  * kDegToRad), sb = std::sin(beta  * kDegToRad);
        const double cg = std::cos(gamma * kDegToRad), sg = std::sin(gamma * kDegToRad);

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
