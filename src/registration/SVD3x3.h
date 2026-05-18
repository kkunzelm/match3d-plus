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
