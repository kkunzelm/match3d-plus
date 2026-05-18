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
