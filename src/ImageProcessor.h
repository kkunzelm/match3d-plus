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

#include <QString>
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
        float    q02      = 0;
        float    q05      = 0;
        float    q10      = 0;
        float    q50      = 0;   // median
        float    q90      = 0;
        float    q95      = 0;
        float    q98      = 0;
        uint32_t validCount = 0;
        // Volume calculations (useful for difference/subtracted images)
        double   positiveVolume = 0;  // Volume above zero (mm³)
        double   negativeVolume = 0;  // Volume below zero (mm³, stored as positive)
        double   pixelArea = 0;       // Area of one pixel (mm²)
        uint32_t positiveCount = 0;   // Number of pixels with z > 0
        uint32_t negativeCount = 0;   // Number of pixels with z < 0
    };
    static Stats computeStats(const ViffImage& img, const RoiMask* roi);

    // Format stats as a tagged key=value text block suitable for display and saving.
    // Each line: "Key = Value\n". Comment lines start with '#'.
    static QString formatStats(const Stats& s, const QString& imageLabel);

    // ── Surface Fitting ──────────────────────────────────────────────────────
    // Plane fit: z = A*x + B*y + C (x, y in world coordinates)
    struct PlaneFit {
        double A = 0;         // Coefficient for x
        double B = 0;         // Coefficient for y
        double C = 0;         // Constant term
        double rmsError = 0;  // RMS residual error
        uint32_t pointCount = 0;
        bool valid = false;
    };
    // Fit a plane to the selected ROI pixels using least squares.
    static PlaneFit fitPlane(const ViffImage& img, const RoiMask* roi);

    // Generate a new image with the fitted plane subtracted from the original.
    static ViffImage subtractPlane(const ViffImage& img, const PlaneFit& fit);

    // Format plane fit results for display.
    static QString formatPlaneFit(const PlaneFit& fit, const QString& imageLabel);

    // Sphere fit: (x-h)^2 + (y-k)^2 + (z-l)^2 = r^2
    struct SphereFit {
        double h = 0;         // Center x coordinate
        double k = 0;         // Center y coordinate
        double l = 0;         // Center z coordinate
        double radius = 0;    // Sphere radius
        double rmsError = 0;  // RMS residual error
        uint32_t pointCount = 0;
        int iterations = 0;   // Number of iterations for convergence
        bool convex = true;   // true=sphere above (z+), false=below (z-)
        bool valid = false;
    };
    // Fit a sphere to the selected ROI pixels using iterative Gauss-Newton.
    static SphereFit fitSphere(const ViffImage& img, const RoiMask* roi);

    // Generate a new image with the fitted sphere subtracted from the original.
    static ViffImage subtractSphere(const ViffImage& img, const SphereFit& fit);

    // Format sphere fit results for display.
    static QString formatSphereFit(const SphereFit& fit, const QString& imageLabel);
};
