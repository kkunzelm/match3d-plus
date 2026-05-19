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

#include "DifferenceCalculator.h"

#include <CCGeom.h>
#include <SquareMatrix.h>

#include <algorithm>
#include <cmath>
#include <limits>

QString DifferenceCalculator::formatStats(const Stats& s, const ViffImage& model, const ViffImage& data) {
    return QString(
        "=== DifferenceCalculator Debug ===\n"
        "Model: %1 x %2, pixelSize: %3 x %4\n"
        "Data:  %5 x %6, pixelSize: %7 x %8\n"
        "---\n"
        "Total model pixels:    %9\n"
        "Valid model pixels:    %10\n"
        "Out of bounds (data):  %11\n"
        "Invalid data pixels:   %12\n"
        "Filtered by min/max:   %13\n"
        "Successful pixels:     %14\n")
        .arg(model.cols).arg(model.rows)
        .arg(model.xPixelSize, 0, 'g', 6).arg(model.yPixelSize, 0, 'g', 6)
        .arg(data.cols).arg(data.rows)
        .arg(data.xPixelSize, 0, 'g', 6).arg(data.yPixelSize, 0, 'g', 6)
        .arg(s.totalModelPixels)
        .arg(s.validModelPixels)
        .arg(s.outOfBoundsData)
        .arg(s.invalidDataPixels)
        .arg(s.filteredByMinMax)
        .arg(s.successfulPixels);
}

// ── Bilinear interpolation ────────────────────────────────────────────────────

float DifferenceCalculator::bilinear(const ViffImage& img, double row, double col) {
    // Snap to nearest integer if very close (avoids interpolation artifacts)
    constexpr double snapEps = 1e-9;
    const double rowRounded = std::round(row);
    const double colRounded = std::round(col);
    if (std::abs(row - rowRounded) < snapEps) row = rowRounded;
    if (std::abs(col - colRounded) < snapEps) col = colRounded;

    const int r0 = static_cast<int>(std::floor(row));
    const int c0 = static_cast<int>(std::floor(col));

    const double dr = row - r0;
    const double dc = col - c0;

    // If exactly on a pixel, just return that pixel value (no interpolation needed)
    if (dr == 0.0 && dc == 0.0) {
        if (r0 < 0 || r0 >= static_cast<int>(img.rows) ||
            c0 < 0 || c0 >= static_cast<int>(img.cols))
            return std::numeric_limits<float>::quiet_NaN();
        const auto ur0 = static_cast<uint32_t>(r0);
        const auto uc0 = static_cast<uint32_t>(c0);
        if (!img.isValid(ur0, uc0))
            return std::numeric_limits<float>::quiet_NaN();
        return img.at(ur0, uc0);
    }

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
    bool useMaxDiff, float maxDiff,
    Stats* stats)
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

    // Debug counters
    Stats localStats;

    for (uint32_t row_m = 0; row_m < model.rows; ++row_m) {
        for (uint32_t col_m = 0; col_m < model.cols; ++col_m) {
            ++localStats.totalModelPixels;

            if (!model.isValid(row_m, col_m)) continue;
            ++localStats.validModelPixels;

            const double x_m = col_m * model.xPixelSize;
            const double y_m = row_m * model.yPixelSize;
            const double z_m = model.at(row_m, col_m);

            // Inverse transform data→model: P_data = R^T * (P_model - T)
            const CCVector3d P_m(x_m, y_m, z_m);
            const CCVector3d P_d = Rt * (P_m - T);

            // Project to data image pixel grid
            if (data.xPixelSize <= 0 || data.yPixelSize <= 0) {
                ++localStats.outOfBoundsData;
                continue;
            }
            const double col_d = P_d.x / data.xPixelSize;
            const double row_d = P_d.y / data.yPixelSize;

            // Check if projected coordinates are within data image bounds
            if (col_d < 0 || col_d >= data.cols - 1 ||
                row_d < 0 || row_d >= data.rows - 1) {
                ++localStats.outOfBoundsData;
                continue;
            }

            // Bilinear interpolation in data image
            const float z_d_raw = bilinear(data, row_d, col_d);
            if (std::isnan(z_d_raw)) {
                ++localStats.invalidDataPixels;
                continue;
            }

            // Transform 3D data point (with interpolated z) to model space
            const CCVector3d P_d_full(P_d.x, P_d.y, static_cast<double>(z_d_raw));
            const CCVector3d P_d_in_model = R * P_d_full + T;

            const float diff = static_cast<float>(z_m - P_d_in_model.z);

            if (useMinDiff && diff < minDiff) {
                ++localStats.filteredByMinMax;
                continue;
            }
            if (useMaxDiff && diff > maxDiff) {
                ++localStats.filteredByMinMax;
                continue;
            }

            ++localStats.successfulPixels;
            result.data[static_cast<size_t>(row_m) * model.cols + col_m] = diff;
        }
    }

    if (stats) *stats = localStats;

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
