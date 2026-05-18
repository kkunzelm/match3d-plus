#include "CoarseRegistration.h"
#include "SVD3x3.h"

#include <CCGeom.h>
#include <PointCloud.h>
#include <RegistrationTools.h>

#include <QString>
#include <cmath>

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
    // =========================================================================
    // 2D Registration for 2.5D Heightmap Data
    // =========================================================================
    // For 2.5D depth images (heightmaps), treating registration as a full 3D
    // problem fails because Z values are orders of magnitude larger than X,Y.
    //
    // Instead, we solve registration in two stages:
    // 1. Solve for X, Y translation and Z-axis rotation (yaw/alpha) using
    //    only the 2D (X,Y) coordinates of the landmark points
    // 2. Compute Z translation as median of Z differences at landmarks
    //
    // This approach is robust because:
    // - The 2D rotation is computed independently of the problematic Z scale
    // - The Z offset is computed directly from corresponding points
    // =========================================================================

    if (dataPts.size() < 2 || refPts.size() < 2) {
        errorMsg = "Need at least 2 point pairs";
        return false;
    }
    if (dataPts.size() != refPts.size()) {
        errorMsg = "Data and reference point counts differ";
        return false;
    }

    // Validate all landmark points have valid Z values
    for (size_t i = 0; i < dataPts.size(); ++i) {
        const int dc = dataPts[i].first;
        const int dr = dataPts[i].second;
        if (dc < 0 || dc >= static_cast<int>(dataImg.cols) ||
            dr < 0 || dr >= static_cast<int>(dataImg.rows) ||
            !dataImg.isValid(static_cast<uint32_t>(dr), static_cast<uint32_t>(dc))) {
            errorMsg = QString("Data point %1 at (%2, %3) has invalid Z value (hole in data)")
                .arg(i + 1).arg(dc).arg(dr);
            return false;
        }

        const int rc = refPts[i].first;
        const int rr = refPts[i].second;
        if (rc < 0 || rc >= static_cast<int>(refImg.cols) ||
            rr < 0 || rr >= static_cast<int>(refImg.rows) ||
            !refImg.isValid(static_cast<uint32_t>(rr), static_cast<uint32_t>(rc))) {
            errorMsg = QString("Reference point %1 at (%2, %3) has invalid Z value (hole in data)")
                .arg(i + 1).arg(rc).arg(rr);
            return false;
        }
    }

    const size_t n = dataPts.size();

    // Step 1: Convert pixel coordinates to world coordinates
    std::vector<CCVector3d> dataWorld(n), refWorld(n);
    for (size_t i = 0; i < n; ++i) {
        CCVector3 dp = pixelToWorld(dataPts[i].first, dataPts[i].second, dataImg);
        CCVector3 rp = pixelToWorld(refPts[i].first,  refPts[i].second,  refImg);
        dataWorld[i] = CCVector3d(dp.x, dp.y, dp.z);
        refWorld[i]  = CCVector3d(rp.x, rp.y, rp.z);
    }

    // Step 2: Compute 2D centroids (X, Y only)
    double dataCx = 0, dataCy = 0, refCx = 0, refCy = 0;
    for (size_t i = 0; i < n; ++i) {
        dataCx += dataWorld[i].x;
        dataCy += dataWorld[i].y;
        refCx  += refWorld[i].x;
        refCy  += refWorld[i].y;
    }
    dataCx /= n;
    dataCy /= n;
    refCx  /= n;
    refCy  /= n;

    // Step 3: Compute optimal 2D rotation angle (least squares)
    // For centered points, the optimal rotation angle θ satisfies:
    //   θ = atan2(Σ(xd*yr - yd*xr), Σ(xd*xr + yd*yr))
    // where (xd, yd) are centered data points, (xr, yr) are centered ref points
    double sumCross = 0;  // Σ(xd*yr - yd*xr)
    double sumDot   = 0;  // Σ(xd*xr + yd*yr)
    for (size_t i = 0; i < n; ++i) {
        const double xd = dataWorld[i].x - dataCx;
        const double yd = dataWorld[i].y - dataCy;
        const double xr = refWorld[i].x  - refCx;
        const double yr = refWorld[i].y  - refCy;
        sumCross += xd * yr - yd * xr;
        sumDot   += xd * xr + yd * yr;
    }
    const double theta = std::atan2(sumCross, sumDot);

    // Step 4: Compute Z offset as median of Z differences
    // After applying 2D rotation and translation, the Z offset is independent
    std::vector<double> zDiffs(n);
    for (size_t i = 0; i < n; ++i) {
        zDiffs[i] = refWorld[i].z - dataWorld[i].z;
    }
    std::sort(zDiffs.begin(), zDiffs.end());
    const double tz = (n % 2 == 0)
        ? (zDiffs[n/2 - 1] + zDiffs[n/2]) / 2.0
        : zDiffs[n/2];

    // Step 5: Compute 2D translation
    // After rotation by θ around origin:
    //   x' = x*cos(θ) - y*sin(θ)
    //   y' = x*sin(θ) + y*cos(θ)
    // Translation: T = refCentroid - R * dataCentroid
    const double cosTheta = std::cos(theta);
    const double sinTheta = std::sin(theta);
    const double rotatedDataCx = dataCx * cosTheta - dataCy * sinTheta;
    const double rotatedDataCy = dataCx * sinTheta + dataCy * cosTheta;
    const double tx = refCx - rotatedDataCx;
    const double ty = refCy - rotatedDataCy;

    // Step 6: Build the transformation
    // For 2.5D data, we only have rotation around Z axis (alpha)
    // The rotation matrix for Z-axis rotation is:
    //   [cos(θ)  -sin(θ)  0]
    //   [sin(θ)   cos(θ)  0]
    //   [  0        0     1]
    result = Transformation3D{};
    result.alpha = theta * 180.0 / M_PI;  // Convert to degrees
    result.beta  = 0.0;
    result.gamma = 0.0;
    result.tx = tx;
    result.ty = ty;
    result.tz = tz;

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
