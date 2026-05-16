#pragma once

#include "../io/ViffReader.h"
#include "Transformation3D.h"

// Computes difference and completed images given a data→model transform.
class DifferenceCalculator {
public:
    // Compute z_model − z_data_in_model_space for each model pixel.
    // transform = data→model (spinbox values from MatchingControlPanel).
    // Output isDiffImage=true, invalid pixels = NaN.
    static ViffImage compute(
        const ViffImage& model,
        const ViffImage& data,
        const Transformation3D& dataToModel,
        bool useMinDiff = false, float minDiff = 0.0f,
        bool useMaxDiff = false, float maxDiff = 0.0f);

    // Warp data image into model pixel grid, filling only invalid model pixels.
    static ViffImage computeCompleted(
        const ViffImage& model,
        const ViffImage& data,
        const Transformation3D& dataToModel);

private:
    // Bilinear interpolation; returns NaN if any corner is out-of-bounds or invalid.
    static float bilinear(const ViffImage& img, double row, double col);
};
