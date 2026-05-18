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
    if (cfg_.useCCLibICP) {
        runCCLibICP();
    } else if (cfg_.use6DOF) {
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
    // Standard point-to-plane ICP in MODEL space
    // For each model point P_m with normal n_m:
    // 1. Find corresponding data point by inverse-transforming to data space, sampling z
    // 2. Transform data point to model space: P_d' = R * P_d + T
    // 3. Minimize: sum of (n_m · (P_d' - P_m))²

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
        // This transforms data -> model
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
        double bvec[6] = {0};
        double sumSqResidual = 0;
        size_t count = 0;

        // Debug counters
        size_t dbgTotal = 0, dbgInvalid = 0, dbgRoi = 0, dbgNeighbor = 0;
        size_t dbgSlope = 0, dbgBounds = 0, dbgDataInvalid = 0, dbgDataRoi = 0;
        size_t dbgOutlier = 0;

        // Outlier threshold from previous iteration
        const double maxResidual = 3.0 * prevRMS;

        for (uint32_t mr = 1; mr + 1 < modelImg.rows; ++mr) {
            for (uint32_t mc = 1; mc + 1 < modelImg.cols; ++mc) {
                ++dbgTotal;
                if (!modelImg.isValid(mr, mc)) { ++dbgInvalid; continue; }
                if (modelRoi && !modelRoi->isSelected(mr, mc)) { ++dbgRoi; continue; }

                // Compute surface normal at model point using central differences
                if (!modelImg.isValid(mr, mc-1) || !modelImg.isValid(mr, mc+1) ||
                    !modelImg.isValid(mr-1, mc) || !modelImg.isValid(mr+1, mc)) {
                    ++dbgNeighbor; continue;
                }

                const double dzdx = (modelImg.at(mr, mc+1) - modelImg.at(mr, mc-1)) /
                                    (2.0 * modelImg.xPixelSize);
                const double dzdy = (modelImg.at(mr+1, mc) - modelImg.at(mr-1, mc)) /
                                    (2.0 * modelImg.yPixelSize);

                // Skip extremely steep slopes (near-vertical surfaces)
                const double gradient = std::sqrt(dzdx*dzdx + dzdy*dzdy);
                const double slopeAngle = std::atan(gradient) * 180.0 / M_PI;
                if (slopeAngle > 89.0) {  // Only filter near-vertical (was 75°)
                    ++dbgSlope; continue;
                }

                // Model point (target) in world coordinates
                const double qx = static_cast<double>(mc) * modelImg.xPixelSize;
                const double qy = static_cast<double>(mr) * modelImg.yPixelSize;
                const double qz = static_cast<double>(modelImg.at(mr, mc));

                // Model surface normal (unnormalized, pointing up)
                // n = (-dz/dx, -dz/dy, 1)
                const double nx = -dzdx;
                const double ny = -dzdy;
                const double nz = 1.0;
                const double nlen = std::sqrt(nx*nx + ny*ny + nz*nz);

                // Inverse transform to find where to sample in data image
                // P_data = R^T * (P_model - T)
                const double dx = qx - T.tx;
                const double dy = qy - T.ty;
                const double dz_approx = qz - T.tz;  // approximate, will refine

                // R^T * (P_m - T) gives data space coordinates
                const double data_x = R[0][0]*dx + R[1][0]*dy + R[2][0]*dz_approx;
                const double data_y = R[0][1]*dx + R[1][1]*dy + R[2][1]*dz_approx;

                // Convert to data pixel coordinates
                const double dc = data_x / dataImg.xPixelSize;
                const double dr = data_y / dataImg.yPixelSize;

                // Check bounds
                if (dc < 0 || dc >= dataImg.cols - 1 || dr < 0 || dr >= dataImg.rows - 1) {
                    ++dbgBounds; continue;
                }

                // Bilinear interpolation to sample z from data image
                const uint32_t c0 = static_cast<uint32_t>(dc);
                const uint32_t r0 = static_cast<uint32_t>(dr);
                const uint32_t c1 = c0 + 1;
                const uint32_t r1 = r0 + 1;

                if (!dataImg.isValid(r0, c0) || !dataImg.isValid(r0, c1) ||
                    !dataImg.isValid(r1, c0) || !dataImg.isValid(r1, c1)) {
                    ++dbgDataInvalid; continue;
                }

                if (dataRoi && (!dataRoi->isSelected(r0, c0) || !dataRoi->isSelected(r0, c1) ||
                               !dataRoi->isSelected(r1, c0) || !dataRoi->isSelected(r1, c1))) {
                    ++dbgDataRoi; continue;
                }

                const double fc = dc - c0;
                const double fr = dr - r0;
                const double data_z = (1-fr) * ((1-fc) * dataImg.at(r0, c0) + fc * dataImg.at(r0, c1))
                                    + fr * ((1-fc) * dataImg.at(r1, c0) + fc * dataImg.at(r1, c1));

                // Data point (source) in data coordinates
                const double px = data_x;
                const double py = data_y;
                const double pz = data_z;

                // Transform data point to model space: P' = R * P + T
                const double px_t = R[0][0]*px + R[0][1]*py + R[0][2]*pz + T.tx;
                const double py_t = R[1][0]*px + R[1][1]*py + R[1][2]*pz + T.ty;
                const double pz_t = R[2][0]*px + R[2][1]*py + R[2][2]*pz + T.tz;

                // Point-to-plane residual in model space: r = n · (P' - Q)
                const double residual = (nx * (px_t - qx) + ny * (py_t - qy) + nz * (pz_t - qz)) / nlen;

                // Outlier detection
                if (std::abs(residual) > maxResidual) {
                    ++dbgOutlier; continue;
                }

                // Linearized point-to-plane ICP Jacobian
                // r ≈ n · (P' - Q) where P' = R*P + T
                // For small angle updates: R ≈ I + [ω]×
                // dr/dω = n · (ω × P) = (P × n) · ω
                // dr/dt = n
                //
                // So Jacobian row = [(P × n), n] for parameters (ωx, ωy, ωz, tx, ty, tz)
                // But we use Euler angles, so we need the chain rule.
                // For small angles: ωx ≈ gamma, ωy ≈ beta, ωz ≈ alpha (in radians)

                // Cross product P × n (using transformed point P')
                const double cpx = py_t * nz - pz_t * ny;  // (P × n).x
                const double cpy = pz_t * nx - px_t * nz;  // (P × n).y
                const double cpz = px_t * ny - py_t * nx;  // (P × n).z

                // Jacobian row: [d/d_gamma, d/d_beta, d/d_alpha, d/d_tx, d/d_ty, d/d_tz]
                // Note: angles are in degrees, so we scale by π/180
                const double deg2rad = M_PI / 180.0;
                double row[6];
                row[0] = cpx / nlen * deg2rad;  // ∂r/∂gamma (rotation around x)
                row[1] = cpy / nlen * deg2rad;  // ∂r/∂beta (rotation around y)
                row[2] = cpz / nlen * deg2rad;  // ∂r/∂alpha (rotation around z)
                row[3] = nx / nlen;              // ∂r/∂tx
                row[4] = ny / nlen;              // ∂r/∂ty
                row[5] = nz / nlen;              // ∂r/∂tz

                // Accumulate normal equations: A += row * row^T, b += row * residual
                for (int i = 0; i < 6; ++i) {
                    for (int j = i; j < 6; ++j) {
                        A[i][j] += row[i] * row[j];
                    }
                    bvec[i] -= row[i] * residual;  // Note: negative because we minimize
                }

                sumSqResidual += residual * residual;
                ++count;
            }
        }

        if (count < 10) {
            emit registrationFinished(false, {}, 0.0, 0,
                QString("Iteration %1: fewer than 10 corresponding points\n"
                        "Debug: total=%2, invalid=%3, roi=%4, neighbor=%5, slope=%6\n"
                        "bounds=%7, dataInvalid=%8, dataRoi=%9, outlier=%10, count=%11")
                    .arg(iter)
                    .arg(dbgTotal).arg(dbgInvalid).arg(dbgRoi).arg(dbgNeighbor).arg(dbgSlope)
                    .arg(dbgBounds).arg(dbgDataInvalid).arg(dbgDataRoi).arg(dbgOutlier).arg(count));
            return;
        }

        // Compute RMS
        const double rms = std::sqrt(sumSqResidual / count);

        // Check for convergence
        if (rms < 1e-6 || (iter > 0 && std::abs(rms - prevRMS) < 1e-6 * rms)) {
            emit registrationFinished(true, T, rms, static_cast<unsigned>(count), QString());
            return;
        }

        // Fill symmetric part of A
        for (int i = 0; i < 6; ++i) {
            for (int j = 0; j < i; ++j) {
                A[i][j] = A[j][i];
            }
        }

        // Solve A * delta = bvec using Cholesky decomposition
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
            // System is singular, stop with current best
            emit registrationFinished(true, T, rms, static_cast<unsigned>(count),
                "Normal equations singular - stopping");
            return;
        }

        // Forward substitution: L * y = bvec
        double y[6];
        for (int i = 0; i < 6; ++i) {
            double sum = bvec[i];
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
        // delta[0-2] are in degrees, delta[3-5] are in mm
        const double angleDelta = std::sqrt(delta[0]*delta[0] + delta[1]*delta[1] + delta[2]*delta[2]);
        const double transDelta = std::sqrt(delta[3]*delta[3] + delta[4]*delta[4] + delta[5]*delta[5]);

        if (angleDelta > 10.0 || transDelta > 50.0) {
            // Solution too large, likely diverging - use damping
            const double damping = 0.1;
            for (int i = 0; i < 6; ++i) delta[i] *= damping;
        }

        // Update transform parameters (deltas are directly in degrees and mm)
        T.gamma += delta[0];
        T.beta  += delta[1];
        T.alpha += delta[2];
        T.tx    += delta[3];
        T.ty    += delta[4];
        T.tz    += delta[5];

        // Check convergence based on update magnitude
        if (angleDelta < 0.001 && transDelta < 0.001 && iter > 0) {
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

// ═══════════════════════════════════════════════════════════════════════════════
// CCCoreLib ICP: Uses the original Besl & McKay ICP from CCCoreLib
// ═══════════════════════════════════════════════════════════════════════════════

void RegistrationWorker::runCCLibICP() {
    const ViffImage& modelImg = cfg_.modelImg;
    const ViffImage& dataImg  = cfg_.dataImg;
    const RoiMask* modelRoi = cfg_.modelRoi.isEmpty() ? nullptr : &cfg_.modelRoi;
    const RoiMask* dataRoi  = cfg_.dataRoi.isEmpty()  ? nullptr : &cfg_.dataRoi;

    // Build point clouds
    CCCoreLib::PointCloud modelCloud = buildCloud(modelImg, modelRoi);
    CCCoreLib::PointCloud dataCloud  = buildCloud(dataImg, dataRoi);

    if (modelCloud.size() < 3) {
        emit registrationFinished(false, {}, 0.0, 0,
            "Reference image: fewer than 3 valid points in ROI");
        return;
    }
    if (dataCloud.size() < 3) {
        emit registrationFinished(false, {}, 0.0, 0,
            "Data image: fewer than 3 valid points in ROI");
        return;
    }

    // Apply initial transform to data cloud
    const Transformation3D& T0 = cfg_.initialTransform;
    const double ca = std::cos(T0.alpha * M_PI / 180.0);
    const double sa = std::sin(T0.alpha * M_PI / 180.0);
    const double cb = std::cos(T0.beta  * M_PI / 180.0);
    const double sb = std::sin(T0.beta  * M_PI / 180.0);
    const double cg = std::cos(T0.gamma * M_PI / 180.0);
    const double sg = std::sin(T0.gamma * M_PI / 180.0);

    // Rotation matrix R = Rz(alpha) * Ry(beta) * Rx(gamma)
    CCCoreLib::SquareMatrix R0(3);
    R0.m_values[0][0] = static_cast<float>(ca * cb);
    R0.m_values[0][1] = static_cast<float>(ca * sb * sg - sa * cg);
    R0.m_values[0][2] = static_cast<float>(ca * sb * cg + sa * sg);
    R0.m_values[1][0] = static_cast<float>(sa * cb);
    R0.m_values[1][1] = static_cast<float>(sa * sb * sg + ca * cg);
    R0.m_values[1][2] = static_cast<float>(sa * sb * cg - ca * sg);
    R0.m_values[2][0] = static_cast<float>(-sb);
    R0.m_values[2][1] = static_cast<float>(cb * sg);
    R0.m_values[2][2] = static_cast<float>(cb * cg);

    CCVector3 T0vec(static_cast<float>(T0.tx),
                    static_cast<float>(T0.ty),
                    static_cast<float>(T0.tz));

    // Apply initial transform to data cloud
    for (unsigned i = 0; i < dataCloud.size(); ++i) {
        CCVector3 p = *dataCloud.getPoint(i);
        CCVector3 p_t = R0 * p + T0vec;
        const_cast<CCVector3*>(dataCloud.getPoint(i))->x = p_t.x;
        const_cast<CCVector3*>(dataCloud.getPoint(i))->y = p_t.y;
        const_cast<CCVector3*>(dataCloud.getPoint(i))->z = p_t.z;
    }

    // Configure CCCoreLib ICP parameters
    CCCoreLib::ICPRegistrationTools::Parameters params;
    params.convType = CCCoreLib::ICPRegistrationTools::MAX_ITER_CONVERGENCE;
    params.nbMaxIterations = static_cast<unsigned>(cfg_.maxIterations);
    params.minRMSDecrease = cfg_.minRMSDecrease;
    params.adjustScale = false;  // Don't adjust scale for dental scans
    params.filterOutFarthestPoints = true;  // Helps with partial overlap
    params.samplingLimit = static_cast<unsigned>(cfg_.samplingLimit);
    params.finalOverlapRatio = cfg_.overlapRatio;
    params.transformationFilters = CCCoreLib::RegistrationTools::SKIP_NONE;  // Full 6-DOF
    params.maxThreadCount = 0;  // Use all available threads

    // Run CCCoreLib ICP
    CCCoreLib::PointProjectionTools::Transformation finalTrans;
    double finalRMS = 0;
    unsigned finalPointCount = 0;

    CCCoreLib::ICPRegistrationTools::RESULT_TYPE result =
        CCCoreLib::ICPRegistrationTools::Register(
            &modelCloud,      // model (reference, won't move)
            nullptr,          // no mesh
            &dataCloud,       // data (will be aligned)
            params,
            finalTrans,
            finalRMS,
            finalPointCount,
            this);            // progress callback

    if (result >= CCCoreLib::ICPRegistrationTools::ICP_ERROR) {
        QString errorMsg;
        switch (result) {
            case CCCoreLib::ICPRegistrationTools::ICP_ERROR_REGISTRATION_STEP:
                errorMsg = "Registration step failed"; break;
            case CCCoreLib::ICPRegistrationTools::ICP_ERROR_DIST_COMPUTATION:
                errorMsg = "Distance computation failed"; break;
            case CCCoreLib::ICPRegistrationTools::ICP_ERROR_NOT_ENOUGH_MEMORY:
                errorMsg = "Not enough memory"; break;
            case CCCoreLib::ICPRegistrationTools::ICP_ERROR_CANCELED_BY_USER:
                errorMsg = "Canceled by user"; break;
            case CCCoreLib::ICPRegistrationTools::ICP_ERROR_INVALID_INPUT:
                errorMsg = "Invalid input"; break;
            default:
                errorMsg = QString("ICP error code %1").arg(static_cast<int>(result));
        }
        emit registrationFinished(false, {}, 0.0, 0, errorMsg);
        return;
    }

    // Convert CCCoreLib transformation back to Transformation3D
    // The CCCoreLib transformation includes the initial transform already applied
    // finalTrans.R is the ICP refinement rotation, finalTrans.T is the translation

    // Combine: Total = finalTrans * initialTrans
    // R_total = R_icp * R_initial
    // T_total = R_icp * T_initial + T_icp
    //
    // But since we pre-applied initialTrans to dataCloud, finalTrans is the total.

    // Extract Euler angles from the rotation matrix (ZYX convention)
    // R = Rz(alpha) * Ry(beta) * Rx(gamma)
    const CCCoreLib::SquareMatrix& R = finalTrans.R;

    double alpha, beta, gamma;
    beta = std::asin(-R.m_values[2][0]);

    if (std::abs(std::cos(beta)) > 1e-6) {
        alpha = std::atan2(R.m_values[1][0], R.m_values[0][0]);
        gamma = std::atan2(R.m_values[2][1], R.m_values[2][2]);
    } else {
        // Gimbal lock
        alpha = std::atan2(-R.m_values[0][1], R.m_values[1][1]);
        gamma = 0;
    }

    Transformation3D result3D;
    result3D.alpha = alpha * 180.0 / M_PI;
    result3D.beta  = beta  * 180.0 / M_PI;
    result3D.gamma = gamma * 180.0 / M_PI;
    result3D.tx = finalTrans.T.x;
    result3D.ty = finalTrans.T.y;
    result3D.tz = finalTrans.T.z;

    emit registrationFinished(true, result3D, finalRMS, finalPointCount, QString());
}
