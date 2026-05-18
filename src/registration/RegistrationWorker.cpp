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
    // =========================================================================
    // 2.5D Registration Refinement Algorithm
    // =========================================================================
    // For 2.5D depth images (heightmaps), standard 3D ICP fails because:
    // 1. Z values are orders of magnitude larger than X,Y
    // 2. Nearest-neighbor correspondences in 3D are wrong for heightmaps
    //
    // Instead, we use a 2.5D-specific approach:
    // 1. Apply initial transform to data image
    // 2. Find overlapping pixels based on (X,Y) grid position
    // 3. Iteratively refine: compute 2D rotation (Z-axis only) and translation
    // 4. Compute Z offset from median of overlapping differences
    // =========================================================================

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
