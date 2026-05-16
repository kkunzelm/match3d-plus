#pragma once

#include <cstdint>

class ViffImage;
class RoiMask;

// Pure static image processing functions.
// All filters work on a copy-then-assign basis (in-place-safe).
// Operations that affect only selected pixels pass an optional RoiMask pointer.
class ImageProcessor {
public:
    // ── Filters ───────────────────────────────────────────────────────────────
    // Replace each valid pixel with the median of valid pixels in its NxN window.
    static void medianFilter(ViffImage& img, int kernelSize);

    // Fill invalid pixels by averaging valid neighbors in NxN window (single pass).
    static void completeFilter(ViffImage& img, int kernelSize);

    // Remove pixels that deviate from the local mean by more than maxDev.
    static void clipOutliers(ViffImage& img, int kernelSize, float maxDev);

    // Remove valid pixels that have fewer than minNeighbors valid 8-neighbours.
    static void thinOut3x3(ViffImage& img, int minNeighbors = 4);

    // Add zero-mean Gaussian noise with given sigma to valid pixels.
    static void addGaussianNoise(ViffImage& img, const RoiMask* roi, float sigma);

    // ── Transforms ────────────────────────────────────────────────────────────
    // Flip the image horizontally (mirror in X).
    static void mirrorX(ViffImage& img);

    // Shift pixel data by (dcol, drow) pixels; add dz to all valid Z values.
    static void shift(ViffImage& img, int dcol, int drow, float dz);

    // Multiply all valid Z values by sz; scale pixel sizes by sx, sy.
    static void scaleZ(ViffImage& img, const RoiMask* roi, float sx, float sy, float sz);

    // ── Z-range operations ────────────────────────────────────────────────────
    // Subtract the minimum valid Z value from all valid pixels.
    static void subtractGlobalMin(ViffImage& img, const RoiMask* roi);

    // Subtract the Z value at the first valid pixel from all valid pixels.
    static void subtractPoint0(ViffImage& img);

    // ── Statistics ────────────────────────────────────────────────────────────
    struct Stats {
        float    min      = 0;
        float    max      = 0;
        float    mean     = 0;
        float    stddev   = 0;
        float    q10      = 0;   // 10th percentile
        float    q90      = 0;   // 90th percentile
        uint32_t validCount = 0;
    };
    static Stats computeStats(const ViffImage& img, const RoiMask* roi);
};
