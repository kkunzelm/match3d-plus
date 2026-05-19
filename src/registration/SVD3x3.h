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

#include <array>
#include <cmath>

// Simple 3x3 SVD implementation using Jacobi rotations.
// Decomposes A = U * S * V^T where U and V are orthogonal and S is diagonal.
class SVD3x3 {
public:
    using Mat3 = std::array<std::array<double, 3>, 3>;
    using Vec3 = std::array<double, 3>;

    // Compute SVD of matrix A.
    // Returns true on success.
    static bool compute(const Mat3& A, Mat3& U, Vec3& S, Mat3& V);

    // Helper: matrix multiplication C = A * B
    static Mat3 multiply(const Mat3& A, const Mat3& B);

    // Helper: transpose
    static Mat3 transpose(const Mat3& A);

    // Helper: determinant
    static double determinant(const Mat3& A);

    // Identity matrix
    static Mat3 identity();

private:
    static constexpr int MAX_ITERATIONS = 50;
    static constexpr double EPSILON = 1e-12;

    // Jacobi rotation to zero out element (p,q)
    static void jacobiRotation(Mat3& A, Mat3& V, int p, int q);
};
