#include "RegistrationWorker.h"

#include <CCGeom.h>
#include <RegistrationTools.h>

#include <algorithm>
#include <cmath>
#include <random>
#include <vector>

RegistrationWorker::RegistrationWorker(Config cfg, QObject* parent)
    : QObject(parent)
    , cfg_(std::move(cfg))
{}

CCCoreLib::PointCloud RegistrationWorker::buildCloud(const ViffImage& img, const RoiMask* roi) {
    CCCoreLib::PointCloud cloud;
    for (uint32_t r = 0; r < img.rows; ++r) {
        for (uint32_t c = 0; c < img.cols; ++c) {
            if (!img.isValid(r, c)) continue;
            if (roi && !roi->isEmpty() && !roi->isSelected(r, c)) continue;
            const float x = static_cast<float>(c) * img.xPixelSize;
            const float y = static_cast<float>(r) * img.yPixelSize;
            const float z = img.at(r, c);
            cloud.addPoint(CCVector3(x, y, z));
        }
    }
    return cloud;
}

void RegistrationWorker::update(float percent) {
    if (percent - lastEmittedPercent_ >= 0.5f) {
        lastEmittedPercent_ = percent;
        emit progressUpdated(percent);
    }
}

void RegistrationWorker::run() {
    if (cfg_.use6DOF) {
        run6DOF();
    } else {
        run4DOF();
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
// 4-DOF Align: Z-axis rotation (alpha) + translation (tx, ty, tz)
// ═══════════════════════════════════════════════════════════════════════════════

void RegistrationWorker::run4DOF() {
    // For 2.5D depth images (heightmaps), standard 3D ICP fails because:
    // 1. Z values are orders of magnitude larger than X,Y
    // 2. Nearest-neighbor correspondences in 3D are wrong for heightmaps
    //
    // Instead, we use a 2.5D-specific approach:
    // 1. Apply initial transform to data image
    // 2. Find overlapping pixels based on (X,Y) grid position
    // 3. Iteratively refine: compute 2D rotation (Z-axis only) and translation
    // 4. Compute Z offset from median of overlapping differences

    const ViffImage& modelImg = cfg_.modelImg;
    const ViffImage& dataImg  = cfg_.dataImg;
    const RoiMask* modelRoi = cfg_.modelRoi.isEmpty() ? nullptr : &cfg_.modelRoi;
    const RoiMask* dataRoi  = cfg_.dataRoi.isEmpty()  ? nullptr : &cfg_.dataRoi;

    // Count valid pixels
    uint64_t modelValidCount = 0, dataValidCount = 0;
    for (uint32_t r = 0; r < modelImg.rows; ++r) {
        for (uint32_t c = 0; c < modelImg.cols; ++c) {
            if (!modelImg.isValid(r, c)) continue;
            if (modelRoi && !modelRoi->isSelected(r, c)) continue;
            ++modelValidCount;
        }
    }
    for (uint32_t r = 0; r < dataImg.rows; ++r) {
        for (uint32_t c = 0; c < dataImg.cols; ++c) {
            if (!dataImg.isValid(r, c)) continue;
            if (dataRoi && !dataRoi->isSelected(r, c)) continue;
            ++dataValidCount;
        }
    }

    if (modelValidCount < 3) {
        emit registrationFinished(false, {}, 0.0, 0,
            "Reference image: fewer than 3 valid pixels in ROI");
        return;
    }
    if (dataValidCount < 3) {
        emit registrationFinished(false, {}, 0.0, 0,
            "Data image: fewer than 3 valid pixels in ROI");
        return;
    }

    // Start with the initial transform
    Transformation3D currentTransform = cfg_.initialTransform;

    // For convergence checking
    double prevRMS = std::numeric_limits<double>::max();
    const int maxIter = std::max(1, cfg_.maxIterations);

    for (int iter = 0; iter < maxIter && !cancelRequested_.load(); ++iter) {
        emit progressUpdated(static_cast<float>(iter * 100.0 / maxIter));

        // Collect corresponding points from overlapping region
        // For each valid model pixel, transform it and check if it lands on valid data
        std::vector<double> modelXs, modelYs, modelZs;
        std::vector<double> dataXs, dataYs, dataZs;

        const double cosA = std::cos(currentTransform.alpha * M_PI / 180.0);
        const double sinA = std::sin(currentTransform.alpha * M_PI / 180.0);

        for (uint32_t mr = 0; mr < modelImg.rows; ++mr) {
            for (uint32_t mc = 0; mc < modelImg.cols; ++mc) {
                if (!modelImg.isValid(mr, mc)) continue;
                if (modelRoi && !modelRoi->isSelected(mr, mc)) continue;

                // Model point in world coordinates
                const double mx = static_cast<double>(mc) * modelImg.xPixelSize;
                const double my = static_cast<double>(mr) * modelImg.yPixelSize;
                const double mz = static_cast<double>(modelImg.at(mr, mc));

                // Inverse transform: where in data image does this model point come from?
                // Forward: data -> model: x' = x*cos - y*sin + tx, y' = x*sin + y*cos + ty
                // Inverse: model -> data: x = (x'-tx)*cos + (y'-ty)*sin, y = -(x'-tx)*sin + (y'-ty)*cos
                const double dxWorld = (mx - currentTransform.tx) * cosA + (my - currentTransform.ty) * sinA;
                const double dyWorld = -(mx - currentTransform.tx) * sinA + (my - currentTransform.ty) * cosA;

                // Convert to data pixel coordinates
                const double dc = dxWorld / dataImg.xPixelSize;
                const double dr = dyWorld / dataImg.yPixelSize;

                // Check bounds
                if (dc < 0 || dc >= dataImg.cols - 1 || dr < 0 || dr >= dataImg.rows - 1)
                    continue;

                // Bilinear interpolation in data image
                const uint32_t c0 = static_cast<uint32_t>(dc);
                const uint32_t r0 = static_cast<uint32_t>(dr);
                const uint32_t c1 = c0 + 1;
                const uint32_t r1 = r0 + 1;

                if (!dataImg.isValid(r0, c0) || !dataImg.isValid(r0, c1) ||
                    !dataImg.isValid(r1, c0) || !dataImg.isValid(r1, c1))
                    continue;

                if (dataRoi && (!dataRoi->isSelected(r0, c0) || !dataRoi->isSelected(r0, c1) ||
                               !dataRoi->isSelected(r1, c0) || !dataRoi->isSelected(r1, c1)))
                    continue;

                const double fc = dc - c0;
                const double fr = dr - r0;
                const double dz = (1-fr) * ((1-fc) * dataImg.at(r0, c0) + fc * dataImg.at(r0, c1))
                                + fr * ((1-fc) * dataImg.at(r1, c0) + fc * dataImg.at(r1, c1));

                // Store corresponding points
                // Model point (target)
                modelXs.push_back(mx);
                modelYs.push_back(my);
                modelZs.push_back(mz);
                // Data point (source, before transform)
                dataXs.push_back(dxWorld);
                dataYs.push_back(dyWorld);
                dataZs.push_back(dz);
            }
        }

        const size_t nCorr = modelXs.size();
        if (nCorr < 3) {
            emit registrationFinished(false, {}, 0.0, 0,
                QString("Iteration %1: fewer than 3 corresponding points").arg(iter));
            return;
        }

        // Subsample if too many correspondences
        std::vector<size_t> indices(nCorr);
        for (size_t i = 0; i < nCorr; ++i) indices[i] = i;

        const size_t maxSamples = static_cast<size_t>(std::max(3, cfg_.samplingLimit));
        if (nCorr > maxSamples) {
            // Shuffle and take first maxSamples
            static std::mt19937 rng(42);  // Fixed seed for reproducibility
            std::shuffle(indices.begin(), indices.end(), rng);
            indices.resize(maxSamples);
        }

        // Compute 2D centroids
        double dataCx = 0, dataCy = 0, modelCx = 0, modelCy = 0;
        for (size_t idx : indices) {
            dataCx += dataXs[idx];
            dataCy += dataYs[idx];
            modelCx += modelXs[idx];
            modelCy += modelYs[idx];
        }
        dataCx /= indices.size();
        dataCy /= indices.size();
        modelCx /= indices.size();
        modelCy /= indices.size();

        // Compute optimal 2D rotation (least squares)
        double sumCross = 0, sumDot = 0;
        for (size_t idx : indices) {
            const double xd = dataXs[idx] - dataCx;
            const double yd = dataYs[idx] - dataCy;
            const double xm = modelXs[idx] - modelCx;
            const double ym = modelYs[idx] - modelCy;
            sumCross += xd * ym - yd * xm;
            sumDot   += xd * xm + yd * ym;
        }
        const double deltaTheta = std::atan2(sumCross, sumDot);

        // Compute Z offset (median of differences)
        std::vector<double> zDiffs;
        zDiffs.reserve(indices.size());
        for (size_t idx : indices) {
            zDiffs.push_back(modelZs[idx] - dataZs[idx] - currentTransform.tz);
        }
        std::sort(zDiffs.begin(), zDiffs.end());
        const double deltaTz = (zDiffs.size() % 2 == 0)
            ? (zDiffs[zDiffs.size()/2 - 1] + zDiffs[zDiffs.size()/2]) / 2.0
            : zDiffs[zDiffs.size()/2];

        // Compute translation update
        const double newCosA = std::cos((currentTransform.alpha * M_PI / 180.0) + deltaTheta);
        const double newSinA = std::sin((currentTransform.alpha * M_PI / 180.0) + deltaTheta);
        const double rotatedDataCx = dataCx * newCosA - dataCy * newSinA;
        const double rotatedDataCy = dataCx * newSinA + dataCy * newCosA;
        const double deltaTx = modelCx - rotatedDataCx - currentTransform.tx;
        const double deltaTy = modelCy - rotatedDataCy - currentTransform.ty;

        // Update transform
        currentTransform.alpha += deltaTheta * 180.0 / M_PI;
        currentTransform.tx += deltaTx;
        currentTransform.ty += deltaTy;
        currentTransform.tz += deltaTz;

        // Compute RMS of Z differences for convergence check
        double sumSqDiff = 0;
        for (size_t idx : indices) {
            const double diff = modelZs[idx] - dataZs[idx] - currentTransform.tz;
            sumSqDiff += diff * diff;
        }
        const double rms = std::sqrt(sumSqDiff / indices.size());

        // Check convergence
        const double rmsDecrease = (prevRMS - rms) / prevRMS;
        if (rmsDecrease < cfg_.minRMSDecrease && iter > 0) {
            // Converged
            emit registrationFinished(true, currentTransform, rms,
                static_cast<unsigned>(indices.size()), QString());
            return;
        }
        prevRMS = rms;
    }

    // Max iterations reached
    const bool canceled = cancelRequested_.load();
    emit registrationFinished(!canceled, currentTransform, prevRMS, 0,
        canceled ? "Canceled by user" : QString());
}

// ═══════════════════════════════════════════════════════════════════════════════
// 6-DOF Refine: Point-to-Plane ICP (Neugebauer algorithm)
// ═══════════════════════════════════════════════════════════════════════════════
// Reference: Peter Neugebauer, "Feinjustierung von Tiefenbildern zur
// Vermessung von kleinen Verformungen", Diplomarbeit, IMMD 5,
// Universität Erlangen-Nürnberg, 1991.
//
// Key idea: Use point-to-plane distance instead of point-to-point.
// This converges faster and handles surface micro-roughness better.
//
// For each model point with surface normal (a, b, c):
//   distance = c * (f - z)
// where f is interpolated height in data image, z is transformed model height.
//
// The linearized constraint equations are:
//   row[0] = (c*y - b*f)    // ∂/∂gamma (rotation around X)
//   row[1] = (a*f - c*x)    // ∂/∂beta  (rotation around Y)
//   row[2] = (b*x - a*y)    // ∂/∂alpha (rotation around Z)
//   row[3] = a              // ∂/∂tx
//   row[4] = b              // ∂/∂ty
//   row[5] = c              // ∂/∂tz
//   row[6] = c*(f - z)      // observation (residual)
// ═══════════════════════════════════════════════════════════════════════════════

void RegistrationWorker::run6DOF() {
    const ViffImage& modelImg = cfg_.modelImg;
    const ViffImage& dataImg  = cfg_.dataImg;
    const RoiMask* modelRoi = cfg_.modelRoi.isEmpty() ? nullptr : &cfg_.modelRoi;
    const RoiMask* dataRoi  = cfg_.dataRoi.isEmpty()  ? nullptr : &cfg_.dataRoi;

    // Count valid pixels
    uint64_t modelValidCount = 0;
    for (uint32_t r = 0; r < modelImg.rows; ++r) {
        for (uint32_t c = 0; c < modelImg.cols; ++c) {
            if (!modelImg.isValid(r, c)) continue;
            if (modelRoi && !modelRoi->isSelected(r, c)) continue;
            ++modelValidCount;
        }
    }

    if (modelValidCount < 10) {
        emit registrationFinished(false, {}, 0.0, 0,
            "Reference image: fewer than 10 valid pixels in ROI");
        return;
    }

    // Start with the initial transform
    Transformation3D T = cfg_.initialTransform;

    // For convergence checking
    double prevRMS = std::numeric_limits<double>::max();
    const int maxIter = std::max(1, cfg_.maxIterations);
    constexpr int DELAY_OF_IMPROVEMENT = 50;
    std::vector<double> qHistory(DELAY_OF_IMPROVEMENT, 1e10);
    int delayIndex = 0;

    for (int iter = 0; iter < maxIter && !cancelRequested_.load(); ++iter) {
        emit progressUpdated(static_cast<float>(iter * 100.0 / maxIter));

        // Compute rotation matrix from current Euler angles (ZYX convention)
        const double ca = std::cos(T.alpha * M_PI / 180.0);
        const double sa = std::sin(T.alpha * M_PI / 180.0);
        const double cb = std::cos(T.beta  * M_PI / 180.0);
        const double sb = std::sin(T.beta  * M_PI / 180.0);
        const double cg = std::cos(T.gamma * M_PI / 180.0);
        const double sg = std::sin(T.gamma * M_PI / 180.0);

        // Rotation matrix R = Rz(alpha) * Ry(beta) * Rx(gamma)
        double R[3][3];
        R[0][0] = ca * cb;
        R[0][1] = ca * sb * sg - sa * cg;
        R[0][2] = ca * sb * cg + sa * sg;
        R[1][0] = sa * cb;
        R[1][1] = sa * sb * sg + ca * cg;
        R[1][2] = sa * sb * cg - ca * sg;
        R[2][0] = -sb;
        R[2][1] = cb * sg;
        R[2][2] = cb * cg;

        // Build normal equations: A * delta = b
        // A is 6x6 symmetric, b is 6x1
        double A[6][6] = {{0}};
        double b[6] = {0};
        double sumSqResidual = 0;
        size_t count = 0;

        // Outlier threshold from previous iteration
        const double maxQ = 2.58 * prevRMS;  // 99% quantile for normal distribution

        for (uint32_t mr = 1; mr + 1 < modelImg.rows; ++mr) {
            for (uint32_t mc = 1; mc + 1 < modelImg.cols; ++mc) {
                if (!modelImg.isValid(mr, mc)) continue;
                if (modelRoi && !modelRoi->isSelected(mr, mc)) continue;

                // Compute surface gradient at model point using central differences
                if (!modelImg.isValid(mr, mc-1) || !modelImg.isValid(mr, mc+1) ||
                    !modelImg.isValid(mr-1, mc) || !modelImg.isValid(mr+1, mc))
                    continue;

                const double zx = (modelImg.at(mr, mc+1) - modelImg.at(mr, mc-1)) /
                                  (2.0 * modelImg.xPixelSize);
                const double zy = (modelImg.at(mr+1, mc) - modelImg.at(mr-1, mc)) /
                                  (2.0 * modelImg.yPixelSize);

                // Skip steep slopes (shadow areas)
                const double norm = std::sqrt(zx*zx + zy*zy + 1.0);
                if (norm > 180.0 / M_PI / 15.0)  // > 75 degree slope
                    continue;

                // Model point in world coordinates
                const double mx_orig = static_cast<double>(mc) * modelImg.xPixelSize;
                const double my_orig = static_cast<double>(mr) * modelImg.yPixelSize;
                const double mz_orig = static_cast<double>(modelImg.at(mr, mc));

                // Transform model point: P' = R * P + T
                const double x = R[0][0]*mx_orig + R[0][1]*my_orig + R[0][2]*mz_orig + T.tx;
                const double y = R[1][0]*mx_orig + R[1][1]*my_orig + R[1][2]*mz_orig + T.ty;
                const double z = R[2][0]*mx_orig + R[2][1]*my_orig + R[2][2]*mz_orig + T.tz;

                // Convert to data pixel coordinates
                const double dc = x / dataImg.xPixelSize;
                const double dr = y / dataImg.yPixelSize;

                // Check bounds
                if (dc < 0 || dc >= dataImg.cols - 1 || dr < 0 || dr >= dataImg.rows - 1)
                    continue;

                // Bilinear interpolation in data image
                const uint32_t c0 = static_cast<uint32_t>(dc);
                const uint32_t r0 = static_cast<uint32_t>(dr);
                const uint32_t c1 = c0 + 1;
                const uint32_t r1 = r0 + 1;

                if (!dataImg.isValid(r0, c0) || !dataImg.isValid(r0, c1) ||
                    !dataImg.isValid(r1, c0) || !dataImg.isValid(r1, c1))
                    continue;

                if (dataRoi && (!dataRoi->isSelected(r0, c0) || !dataRoi->isSelected(r0, c1) ||
                               !dataRoi->isSelected(r1, c0) || !dataRoi->isSelected(r1, c1)))
                    continue;

                const double fc = dc - c0;
                const double fr = dr - r0;
                const double f = (1-fr) * ((1-fc) * dataImg.at(r0, c0) + fc * dataImg.at(r0, c1))
                               + fr * ((1-fc) * dataImg.at(r1, c0) + fc * dataImg.at(r1, c1));

                // Transform surface normal: n' = R * n (normals transform by rotation only)
                // Original normal: (-zx, -zy, 1) / norm
                const double nx = -zx / norm;
                const double ny = -zy / norm;
                const double nz = 1.0 / norm;

                // Transformed normal components
                const double a = R[0][0]*nx + R[0][1]*ny + R[0][2]*nz;
                const double bc = R[1][0]*nx + R[1][1]*ny + R[1][2]*nz;
                const double c = R[2][0]*nx + R[2][1]*ny + R[2][2]*nz;

                // Out of sight? (normal pointing away)
                if (c < 0.0)
                    continue;

                // Point-to-plane residual
                const double residual = c * (f - z);

                // Outlier detection
                if (std::abs(residual) > maxQ)
                    continue;

                // Build row of the design matrix (Neugebauer's formulation)
                double row[6];
                row[0] = c * y - bc * f;    // ∂/∂gamma
                row[1] = a * f - c * x;     // ∂/∂beta
                row[2] = bc * x - a * y;    // ∂/∂alpha
                row[3] = a;                  // ∂/∂tx
                row[4] = bc;                 // ∂/∂ty
                row[5] = c;                  // ∂/∂tz

                // Accumulate normal equations: A += row * row^T, b += row * residual
                for (int i = 0; i < 6; ++i) {
                    for (int j = i; j < 6; ++j) {
                        A[i][j] += row[i] * row[j];
                    }
                    b[i] += row[i] * residual;
                }

                sumSqResidual += residual * residual;
                ++count;
            }
        }

        if (count < 10) {
            emit registrationFinished(false, {}, 0.0, 0,
                QString("Iteration %1: fewer than 10 corresponding points").arg(iter));
            return;
        }

        // Compute RMS
        const double rms = std::sqrt(sumSqResidual / count);

        // Check for improvement (ring buffer of last N iterations)
        if (rms >= qHistory[delayIndex]) {
            // No improvement in the last DELAY_OF_IMPROVEMENT iterations
            emit registrationFinished(true, T, rms, static_cast<unsigned>(count), QString());
            return;
        }
        qHistory[delayIndex] = rms;
        delayIndex = (delayIndex + 1) % DELAY_OF_IMPROVEMENT;

        // Fill symmetric part of A
        for (int i = 0; i < 6; ++i) {
            for (int j = 0; j < i; ++j) {
                A[i][j] = A[j][i];
            }
        }

        // Solve A * delta = b using Cholesky decomposition
        // First, Cholesky factorization: A = L * L^T
        double L[6][6] = {{0}};
        bool singular = false;
        for (int i = 0; i < 6 && !singular; ++i) {
            for (int j = 0; j <= i; ++j) {
                double sum = A[i][j];
                for (int k = 0; k < j; ++k) {
                    sum -= L[i][k] * L[j][k];
                }
                if (i == j) {
                    if (sum <= 0) {
                        singular = true;
                        break;
                    }
                    L[i][j] = std::sqrt(sum);
                } else {
                    L[i][j] = sum / L[j][j];
                }
            }
        }

        if (singular) {
            // System is singular, stop
            emit registrationFinished(true, T, rms, static_cast<unsigned>(count),
                "Normal equations singular - stopping");
            return;
        }

        // Forward substitution: L * y = b
        double y[6];
        for (int i = 0; i < 6; ++i) {
            double sum = b[i];
            for (int j = 0; j < i; ++j) {
                sum -= L[i][j] * y[j];
            }
            y[i] = sum / L[i][i];
        }

        // Back substitution: L^T * delta = y
        double delta[6];
        for (int i = 5; i >= 0; --i) {
            double sum = y[i];
            for (int j = i + 1; j < 6; ++j) {
                sum -= L[j][i] * delta[j];
            }
            delta[i] = sum / L[i][i];
        }

        // Check solution magnitude (prevent divergence)
        double solutionNorm = 0;
        for (int i = 0; i < 6; ++i) {
            solutionNorm += delta[i] * delta[i];
        }
        solutionNorm = std::sqrt(solutionNorm);

        if (solutionNorm > 20.0) {
            // Solution too large, likely diverging
            emit registrationFinished(true, T, rms, static_cast<unsigned>(count),
                "Solution diverging - stopping");
            return;
        }

        // Update transform parameters
        T.gamma += delta[0] * 180.0 / M_PI;
        T.beta  += delta[1] * 180.0 / M_PI;
        T.alpha += delta[2] * 180.0 / M_PI;
        T.tx    += delta[3];
        T.ty    += delta[4];
        T.tz    += delta[5];

        // Check convergence based on solution norm
        if (solutionNorm < cfg_.minRMSDecrease && iter > 0) {
            emit registrationFinished(true, T, rms, static_cast<unsigned>(count), QString());
            return;
        }

        prevRMS = rms;
    }

    // Max iterations reached
    const bool canceled = cancelRequested_.load();
    emit registrationFinished(!canceled, T, prevRMS, 0,
        canceled ? "Canceled by user" : QString());
}
