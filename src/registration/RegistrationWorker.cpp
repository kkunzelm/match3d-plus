#include "RegistrationWorker.h"

#include <CCGeom.h>
#include <RegistrationTools.h>

#include <algorithm>
#include <cmath>

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
    // Build point clouds on worker thread (avoids blocking UI)
    const RoiMask* modelRoi = cfg_.modelRoi.isEmpty() ? nullptr : &cfg_.modelRoi;
    const RoiMask* dataRoi  = cfg_.dataRoi.isEmpty()  ? nullptr : &cfg_.dataRoi;

    auto modelCloud = buildCloud(cfg_.modelImg, modelRoi);
    if (modelCloud.size() < 3) {
        emit registrationFinished(false, {}, 0.0, 0,
            "Reference image: fewer than 3 valid pixels in ROI");
        return;
    }

    auto dataCloud = buildCloud(cfg_.dataImg, dataRoi);
    if (dataCloud.size() < 3) {
        emit registrationFinished(false, {}, 0.0, 0,
            "Data image: fewer than 3 valid pixels in ROI");
        return;
    }

    // Pre-apply initial coarse transform to data cloud
    const CCCoreLib::PointProjectionTools::Transformation initTrans =
        cfg_.initialTransform.toCCTransform();
    initTrans.apply(dataCloud);

    // ICP requires an active scalar field on the data cloud
    if (!dataCloud.enableScalarField()) {
        emit registrationFinished(false, {}, 0.0, 0,
            "Failed to enable scalar field on data cloud");
        return;
    }
    dataCloud.setCurrentOutScalarField(dataCloud.getCurrentInScalarFieldIndex());

    // ICP parameters
    CCCoreLib::ICPRegistrationTools::Parameters icpParams;
    icpParams.convType                = CCCoreLib::ICPRegistrationTools::MAX_ITER_CONVERGENCE;
    icpParams.nbMaxIterations         = static_cast<unsigned>(std::max(1, cfg_.maxIterations));
    icpParams.samplingLimit           = static_cast<unsigned>(std::max(3, cfg_.samplingLimit));
    icpParams.finalOverlapRatio       = std::clamp(cfg_.overlapRatio, 0.01, 1.0);
    icpParams.filterOutFarthestPoints = (cfg_.overlapRatio < 0.999);
    icpParams.adjustScale             = false;
    icpParams.minRMSDecrease          = cfg_.minRMSDecrease;
    icpParams.maxThreadCount          = 0;  // use all available cores

    CCCoreLib::PointProjectionTools::Transformation fineTransform;
    double   finalRMS   = 0.0;
    unsigned finalCount = 0;

    const auto result = CCCoreLib::ICPRegistrationTools::Register(
        &modelCloud, nullptr, &dataCloud,
        icpParams,
        fineTransform,
        finalRMS,
        finalCount,
        this);

    const bool canceled = cancelRequested_.load();
    const bool success =
        (result == CCCoreLib::ICPRegistrationTools::ICP_APPLY_TRANSFO ||
         result == CCCoreLib::ICPRegistrationTools::ICP_NOTHING_TO_DO);

    if (!success && !canceled) {
        emit registrationFinished(false, {}, 0.0, 0,
            QString("ICP failed (code %1)").arg(static_cast<int>(result)));
        return;
    }

    // Compose total transform: fineTransform ∘ initialTransform
    // P_final = R_fine * (R_init * P_orig + T_init) + T_fine
    //         = (R_fine * R_init) * P_orig + (R_fine * T_init + T_fine)
    CCCoreLib::PointProjectionTools::Transformation totalTrans;
    totalTrans.s = 1.0;
    if (result == CCCoreLib::ICPRegistrationTools::ICP_NOTHING_TO_DO) {
        totalTrans = initTrans;  // ICP converged with no movement
    } else {
        totalTrans.R = fineTransform.R * initTrans.R;
        totalTrans.T = fineTransform.R * initTrans.T + fineTransform.T;
    }

    emit registrationFinished(
        !canceled,
        Transformation3D::fromCCTransform(totalTrans),
        finalRMS,
        finalCount,
        canceled ? "Canceled by user" : QString());
}
