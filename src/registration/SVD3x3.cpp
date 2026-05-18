#include "SVD3x3.h"
#include <algorithm>

SVD3x3::Mat3 SVD3x3::identity() {
    return {{{1, 0, 0}, {0, 1, 0}, {0, 0, 1}}};
}

SVD3x3::Mat3 SVD3x3::multiply(const Mat3& A, const Mat3& B) {
    Mat3 C = {{{0, 0, 0}, {0, 0, 0}, {0, 0, 0}}};
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            for (int k = 0; k < 3; ++k) {
                C[i][j] += A[i][k] * B[k][j];
            }
        }
    }
    return C;
}

SVD3x3::Mat3 SVD3x3::transpose(const Mat3& A) {
    Mat3 At;
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            At[i][j] = A[j][i];
        }
    }
    return At;
}

double SVD3x3::determinant(const Mat3& A) {
    return A[0][0] * (A[1][1] * A[2][2] - A[1][2] * A[2][1])
         - A[0][1] * (A[1][0] * A[2][2] - A[1][2] * A[2][0])
         + A[0][2] * (A[1][0] * A[2][1] - A[1][1] * A[2][0]);
}

void SVD3x3::jacobiRotation(Mat3& A, Mat3& V, int p, int q) {
    if (std::abs(A[p][q]) < EPSILON) return;

    double tau = (A[q][q] - A[p][p]) / (2.0 * A[p][q]);
    double t = (tau >= 0 ? 1.0 : -1.0) / (std::abs(tau) + std::sqrt(1.0 + tau * tau));
    double c = 1.0 / std::sqrt(1.0 + t * t);
    double s = t * c;

    // Update A = J^T * A * J
    double App = A[p][p];
    double Aqq = A[q][q];
    double Apq = A[p][q];

    A[p][p] = c * c * App - 2 * c * s * Apq + s * s * Aqq;
    A[q][q] = s * s * App + 2 * c * s * Apq + c * c * Aqq;
    A[p][q] = A[q][p] = 0;

    for (int k = 0; k < 3; ++k) {
        if (k != p && k != q) {
            double Akp = A[k][p];
            double Akq = A[k][q];
            A[k][p] = A[p][k] = c * Akp - s * Akq;
            A[k][q] = A[q][k] = s * Akp + c * Akq;
        }
    }

    // Update V = V * J
    for (int k = 0; k < 3; ++k) {
        double Vkp = V[k][p];
        double Vkq = V[k][q];
        V[k][p] = c * Vkp - s * Vkq;
        V[k][q] = s * Vkp + c * Vkq;
    }
}

bool SVD3x3::compute(const Mat3& A, Mat3& U, Vec3& S, Mat3& V) {
    // SVD via eigendecomposition of A^T * A
    // A = U * S * V^T
    // A^T * A = V * S^2 * V^T (eigendecomposition)
    // Then U = A * V * S^-1

    Mat3 AtA = multiply(transpose(A), A);
    V = identity();

    // Jacobi eigendecomposition of AtA
    for (int iter = 0; iter < MAX_ITERATIONS; ++iter) {
        double off = 0;
        for (int i = 0; i < 3; ++i) {
            for (int j = i + 1; j < 3; ++j) {
                off += AtA[i][j] * AtA[i][j];
            }
        }
        if (off < EPSILON * EPSILON) break;

        for (int p = 0; p < 2; ++p) {
            for (int q = p + 1; q < 3; ++q) {
                jacobiRotation(AtA, V, p, q);
            }
        }
    }

    // Singular values are sqrt of eigenvalues
    for (int i = 0; i < 3; ++i) {
        S[i] = std::sqrt(std::max(0.0, AtA[i][i]));
    }

    // Sort singular values in descending order
    for (int i = 0; i < 2; ++i) {
        for (int j = i + 1; j < 3; ++j) {
            if (S[j] > S[i]) {
                std::swap(S[i], S[j]);
                for (int k = 0; k < 3; ++k) {
                    std::swap(V[k][i], V[k][j]);
                }
            }
        }
    }

    // Compute U = A * V * S^-1
    Mat3 AV = multiply(A, V);
    U = identity();
    for (int j = 0; j < 3; ++j) {
        if (S[j] > EPSILON) {
            for (int i = 0; i < 3; ++i) {
                U[i][j] = AV[i][j] / S[j];
            }
        } else {
            // Handle zero singular value - use arbitrary orthogonal vector
            // For simplicity, keep identity column
        }
    }

    // Ensure U is orthogonal (Gram-Schmidt on columns)
    // Column 0
    double norm0 = std::sqrt(U[0][0]*U[0][0] + U[1][0]*U[1][0] + U[2][0]*U[2][0]);
    if (norm0 > EPSILON) {
        U[0][0] /= norm0; U[1][0] /= norm0; U[2][0] /= norm0;
    }
    // Column 1 - orthogonalize against column 0
    double dot01 = U[0][0]*U[0][1] + U[1][0]*U[1][1] + U[2][0]*U[2][1];
    U[0][1] -= dot01 * U[0][0];
    U[1][1] -= dot01 * U[1][0];
    U[2][1] -= dot01 * U[2][0];
    double norm1 = std::sqrt(U[0][1]*U[0][1] + U[1][1]*U[1][1] + U[2][1]*U[2][1]);
    if (norm1 > EPSILON) {
        U[0][1] /= norm1; U[1][1] /= norm1; U[2][1] /= norm1;
    }
    // Column 2 - cross product of columns 0 and 1
    U[0][2] = U[1][0]*U[2][1] - U[2][0]*U[1][1];
    U[1][2] = U[2][0]*U[0][1] - U[0][0]*U[2][1];
    U[2][2] = U[0][0]*U[1][1] - U[1][0]*U[0][1];

    return true;
}
