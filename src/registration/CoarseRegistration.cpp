#include "CoarseRegistration.h"

#include <CCGeom.h>
#include <PointCloud.h>
#include <RegistrationTools.h>

#include <QString>

CCVector3 CoarseRegistration::pixelToWorld(int col, int row,
                                           const ViffImage& img)
{
    // World coordinates: X = col * xPixelSize, Y = row * yPixelSize, Z = depth
    const float x = static_cast<float>(col) * img.xPixelSize;
    const float y = static_cast<float>(row) * img.yPixelSize;
    const float z = img.isValid(static_cast<uint32_t>(row),
                                static_cast<uint32_t>(col))
                    ? img.at(static_cast<uint32_t>(row),
                             static_cast<uint32_t>(col))
                    : 0.0f;
    return CCVector3(x, y, z);
}

bool CoarseRegistration::fromPoints(
    const std::vector<std::pair<int,int>>& dataPts,
    const ViffImage& dataImg,
    const std::vector<std::pair<int,int>>& refPts,
    const ViffImage& refImg,
    Transformation3D& result,
    QString& errorMsg)
{
    if (dataPts.size() < 3 || refPts.size() < 3) {
        errorMsg = "Need at least 3 point pairs";
        return false;
    }
    if (dataPts.size() != refPts.size()) {
        errorMsg = "Data and reference point counts differ";
        return false;
    }

    CCCoreLib::PointCloud dataCloud;
    CCCoreLib::PointCloud refCloud;
    for (size_t i = 0; i < dataPts.size(); ++i) {
        dataCloud.addPoint(pixelToWorld(dataPts[i].first, dataPts[i].second, dataImg));
        refCloud.addPoint(pixelToWorld(refPts[i].first,   refPts[i].second,  refImg));
    }

    CCCoreLib::PointProjectionTools::Transformation trans;
    if (!CCCoreLib::HornRegistrationTools::FindAbsoluteOrientation(
            &dataCloud, &refCloud, trans, /*fixedScale=*/true)) {
        errorMsg = "FindAbsoluteOrientation failed";
        return false;
    }

    result = Transformation3D::fromCCTransform(trans);
    return true;
}

bool CoarseRegistration::fromCOM(
    const ViffImage& dataImg, const RoiMask* dataRoi,
    const ViffImage& refImg,  const RoiMask* refRoi,
    Transformation3D& result,
    QString& errorMsg)
{
    auto computeCOM = [](const ViffImage& img, const RoiMask* roi,
                         double& cx, double& cy, double& cz) -> bool {
        double sx = 0, sy = 0, sz = 0;
        uint64_t cnt = 0;
        for (uint32_t r = 0; r < img.rows; ++r) {
            for (uint32_t c = 0; c < img.cols; ++c) {
                if (!img.isValid(r, c)) continue;
                if (roi && !roi->isSelected(r, c)) continue;
                sx += static_cast<double>(c) * img.xPixelSize;
                sy += static_cast<double>(r) * img.yPixelSize;
                sz += static_cast<double>(img.at(r, c));
                ++cnt;
            }
        }
        if (cnt == 0) return false;
        cx = sx / cnt; cy = sy / cnt; cz = sz / cnt;
        return true;
    };

    double dcx, dcy, dcz, rcx, rcy, rcz;
    if (!computeCOM(dataImg, dataRoi, dcx, dcy, dcz)) {
        errorMsg = "Data image has no valid ROI pixels";
        return false;
    }
    if (!computeCOM(refImg, refRoi, rcx, rcy, rcz)) {
        errorMsg = "Reference image has no valid ROI pixels";
        return false;
    }

    result = Transformation3D{};
    result.tx = rcx - dcx;
    result.ty = rcy - dcy;
    result.tz = rcz - dcz;
    return true;
}
