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

#pragma once

#include "../io/ViffReader.h"
#include "Transformation3D.h"

#include <QString>

// Computes difference and completed images given a data→model transform.
class DifferenceCalculator {
public:
    // Debug statistics from compute()
    struct Stats {
        uint64_t totalModelPixels = 0;
        uint64_t validModelPixels = 0;
        uint64_t outOfBoundsData = 0;
        uint64_t invalidDataPixels = 0;
        uint64_t filteredByMinMax = 0;
        uint64_t successfulPixels = 0;
    };

    // Compute z_model − z_data_in_model_space for each model pixel.
    // transform = data→model (spinbox values from MatchingControlPanel).
    // ROI filtering is NOT applied here - difference is computed for ALL valid pixels.
    // Use the ROI on the resulting difference image for analysis/statistics.
    // Output isDiffImage=true, invalid pixels = NaN.
    // If stats is non-null, debug statistics are written there.
    static ViffImage compute(
        const ViffImage& model,
        const ViffImage& data,
        const Transformation3D& dataToModel,
        bool useMinDiff = false, float minDiff = 0.0f,
        bool useMaxDiff = false, float maxDiff = 0.0f,
        Stats* stats = nullptr);

    // Format stats as human-readable string
    static QString formatStats(const Stats& s, const ViffImage& model, const ViffImage& data);

    // Warp data image into model pixel grid, filling only invalid model pixels.
    static ViffImage computeCompleted(
        const ViffImage& model,
        const ViffImage& data,
        const Transformation3D& dataToModel);

private:
    // Bilinear interpolation; returns NaN if any corner is out-of-bounds or invalid.
    static float bilinear(const ViffImage& img, double row, double col);
};
