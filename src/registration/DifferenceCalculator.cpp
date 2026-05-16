#include "DifferenceCalculator.h"

#include <CCGeom.h>
#include <SquareMatrix.h>

#include <algorithm>
#include <cmath>
#include <limits>

// ── Bilinear interpolation ────────────────────────────────────────────────────

float DifferenceCalculator::bilinear(const ViffImage& img, double row, double col) {
    const int r0 = static_cast<int>(row);
    const int c0 = static_cast<int>(col);
    const int r1 = r0 + 1;
    const int c1 = c0 + 1;

    if (r0 < 0 || r1 >= static_cast<int>(img.rows) ||
        c0 < 0 || c1 >= static_cast<int>(img.cols))
        return std::numeric_limits<float>::quiet_NaN();

    const auto ur0 = static_cast<uint32_t>(r0);
    const auto uc0 = static_cast<uint32_t>(c0);
    const auto ur1 = static_cast<uint32_t>(r1);
    const auto uc1 = static_cast<uint32_t>(c1);

    if (!img.isValid(ur0, uc0) || !img.isValid(ur0, uc1) ||
        !img.isValid(ur1, uc0) || !img.isValid(ur1, uc1))
        return std::numeric_limits<float>::quiet_NaN();

    const double dr = row - r0;
    const double dc = col - c0;

    return static_cast<float>(
        (1.0 - dr) * (1.0 - dc) * img.at(ur0, uc0) +
        (1.0 - dr) *         dc  * img.at(ur0, uc1) +
               dr  * (1.0 - dc) * img.at(ur1, uc0) +
               dr  *         dc  * img.at(ur1, uc1));
}

// ── Difference image ──────────────────────────────────────────────────────────

ViffImage DifferenceCalculator::compute(
    const ViffImage& model,
    const ViffImage& data,
    const Transformation3D& dataToModel,
    bool useMinDiff, float minDiff,
    bool useMaxDiff, float maxDiff)
{
    const auto ccT = dataToModel.toCCTransform();
    const CCCoreLib::SquareMatrixd& R = ccT.R;
    const CCVector3d& T = ccT.T;
    const CCCoreLib::SquareMatrixd Rt = R.transposed();

    ViffImage result;
    result.rows        = model.rows;
    result.cols        = model.cols;
    result.xPixelSize  = model.xPixelSize;
    result.yPixelSize  = model.yPixelSize;
    result.isDiffImage = true;
    result.data.assign(static_cast<size_t>(model.rows) * model.cols,
                       std::numeric_limits<float>::quiet_NaN());

    for (uint32_t row_m = 0; row_m < model.rows; ++row_m) {
        for (uint32_t col_m = 0; col_m < model.cols; ++col_m) {
            if (!model.isValid(row_m, col_m)) continue;

            const double x_m = col_m * model.xPixelSize;
            const double y_m = row_m * model.yPixelSize;
            const double z_m = model.at(row_m, col_m);

            // Inverse transform data→model: P_data = R^T * (P_model - T)
            const CCVector3d P_m(x_m, y_m, z_m);
            const CCVector3d P_d = Rt * (P_m - T);

            // Project to data image pixel grid
            if (data.xPixelSize <= 0 || data.yPixelSize <= 0) continue;
            const double col_d = P_d.x / data.xPixelSize;
            const double row_d = P_d.y / data.yPixelSize;

            // Bilinear interpolation in data image
            const float z_d_raw = bilinear(data, row_d, col_d);
            if (std::isnan(z_d_raw)) continue;

            // Transform 3D data point (with interpolated z) to model space
            const CCVector3d P_d_full(P_d.x, P_d.y, static_cast<double>(z_d_raw));
            const CCVector3d P_d_in_model = R * P_d_full + T;

            const float diff = static_cast<float>(z_m - P_d_in_model.z);

            if (useMinDiff && diff < minDiff) continue;
            if (useMaxDiff && diff > maxDiff) continue;

            result.data[static_cast<size_t>(row_m) * model.cols + col_m] = diff;
        }
    }

    return result;
}

// ── Completed image ───────────────────────────────────────────────────────────

ViffImage DifferenceCalculator::computeCompleted(
    const ViffImage& model,
    const ViffImage& data,
    const Transformation3D& dataToModel)
{
    const auto ccT = dataToModel.toCCTransform();
    const CCCoreLib::SquareMatrixd& R = ccT.R;
    const CCVector3d& T = ccT.T;

    ViffImage result = model;  // copy model as base

    // Forward-project each data pixel into model space and fill invalid model pixels.
    for (uint32_t row_d = 0; row_d < data.rows; ++row_d) {
        for (uint32_t col_d = 0; col_d < data.cols; ++col_d) {
            if (!data.isValid(row_d, col_d)) continue;

            const double x_d = col_d * data.xPixelSize;
            const double y_d = row_d * data.yPixelSize;
            const double z_d = data.at(row_d, col_d);

            const CCVector3d P_d(x_d, y_d, z_d);
            const CCVector3d P_m = R * P_d + T;

            // Round to nearest model pixel
            const int col_m = static_cast<int>(std::round(P_m.x / model.xPixelSize));
            const int row_m = static_cast<int>(std::round(P_m.y / model.yPixelSize));

            if (col_m < 0 || col_m >= static_cast<int>(model.cols) ||
                row_m < 0 || row_m >= static_cast<int>(model.rows)) continue;

            const auto ucm = static_cast<uint32_t>(col_m);
            const auto urm = static_cast<uint32_t>(row_m);

            if (!model.isValid(urm, ucm))
                result.data[static_cast<size_t>(urm) * model.cols + ucm] =
                    static_cast<float>(P_m.z);
        }
    }

    return result;
}
