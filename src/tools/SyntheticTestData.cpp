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

// Synthetic test data generator for registration testing
// Creates non-symmetric surfaces with known transformations and noise

#include "../io/ViffReader.h"
#include "../io/ViffWriter.h"

#include <cmath>
#include <iostream>
#include <numbers>
#include <random>
#include <string>

namespace {
    constexpr double kDegToRad = std::numbers::pi / 180.0;
}

// Generate a highly non-symmetric surface
// Combines: tilted plane + asymmetric gaussian bumps + a ridge
ViffImage generateNonSymmetricSurface(uint32_t rows, uint32_t cols,
                                       float pixelSize, float baseHeight) {
    ViffImage img;
    img.rows = rows;
    img.cols = cols;
    img.xPixelSize = pixelSize;
    img.yPixelSize = pixelSize;
    img.data.resize(static_cast<size_t>(rows) * cols);

    const float cx = cols * pixelSize / 2.0f;  // center x
    const float cy = rows * pixelSize / 2.0f;  // center y

    for (uint32_t r = 0; r < rows; ++r) {
        for (uint32_t c = 0; c < cols; ++c) {
            const float x = c * pixelSize;
            const float y = r * pixelSize;

            // 1. Tilted base plane (asymmetric tilt)
            float z = baseHeight + 0.02f * x + 0.01f * y;

            // 2. Large asymmetric gaussian bump (off-center)
            const float bx1 = cx + 0.2f * cx;  // bump center offset from image center
            const float by1 = cy - 0.3f * cy;
            const float dx1 = x - bx1;
            const float dy1 = y - by1;
            z += 5.0f * std::exp(-(dx1*dx1 / (0.1f*cx*cx) + dy1*dy1 / (0.15f*cy*cy)));

            // 3. Smaller bump in different location
            const float bx2 = cx - 0.4f * cx;
            const float by2 = cy + 0.25f * cy;
            const float dx2 = x - bx2;
            const float dy2 = y - by2;
            z += 2.5f * std::exp(-(dx2*dx2 + dy2*dy2) / (0.05f * cx * cy));

            // 4. Diagonal ridge (asymmetric)
            const float ridgeDist = std::abs(0.7f * (x - cx) - 0.3f * (y - cy));
            z += 1.5f * std::exp(-ridgeDist * ridgeDist / (0.02f * cx * cx));

            // 5. Depression/valley in one corner
            const float vx = 0.15f * cols * pixelSize;
            const float vy = 0.85f * rows * pixelSize;
            const float dvx = x - vx;
            const float dvy = y - vy;
            z -= 3.0f * std::exp(-(dvx*dvx + dvy*dvy) / (0.03f * cx * cy));

            img.data[r * cols + c] = z;
        }
    }

    return img;
}

// Apply 6-DOF transformation to create "data" image from "model"
// Convention: T transforms data->model, so P_model = R * P_data + T
// To create data from model: P_data = R^T * (P_model - T)
// Uses backward mapping for continuous output (no holes)
ViffImage applyTransformation(const ViffImage& model,
                               float alpha_deg, float beta_deg, float gamma_deg,
                               float tx, float ty, float tz,
                               std::mt19937& rng, float noiseStdDev) {
    // Create rotation matrix R = Rz(alpha) * Ry(beta) * Rx(gamma)
    const double ca = std::cos(alpha_deg * kDegToRad);
    const double sa = std::sin(alpha_deg * kDegToRad);
    const double cb = std::cos(beta_deg * kDegToRad);
    const double sb = std::sin(beta_deg * kDegToRad);
    const double cg = std::cos(gamma_deg * kDegToRad);
    const double sg = std::sin(gamma_deg * kDegToRad);

    double R[3][3];
    R[0][0] = ca * cb;
    R[0][1] = ca * sb * sg - sa * cg;
    R[0][2] = ca * sb * cg + sa * sg;
    R[1][0] = sa * cb;
    R[1][1] = sa * sb * sg + ca * cg;
    R[1][2] = sa * sb * cg - ca * sg;
    R[2][0] = -sb;
    R[2][1] = cb * sg;
    R[2][2] = cb * cg;

    // Output image (same size as model)
    ViffImage data;
    data.rows = model.rows;
    data.cols = model.cols;
    data.xPixelSize = model.xPixelSize;
    data.yPixelSize = model.yPixelSize;
    data.data.resize(static_cast<size_t>(model.rows) * model.cols,
                     std::numeric_limits<float>::quiet_NaN());

    std::normal_distribution<float> noise(0.0f, noiseStdDev);

    // Backward mapping: for each data pixel, find corresponding model pixel
    // P_model = R * P_data + T  =>  P_data = R^T * (P_model - T)
    // But we iterate over data pixels, so we need forward: P_model = R * P_data + T

    for (uint32_t r = 0; r < data.rows; ++r) {
        for (uint32_t c = 0; c < data.cols; ++c) {
            // Data pixel position in world coordinates
            const double x_data = c * data.xPixelSize;
            const double y_data = r * data.yPixelSize;

            // For 2.5D heightmaps, we need to find z_data such that when transformed,
            // the point lands on the model surface.
            // This is complex because z affects the transformation.
            //
            // Approximation for small rotations:
            // First find (x_model, y_model) ignoring z contribution,
            // sample z_model, then compute z_data.

            // For small beta and gamma, the z contribution to x,y is small.
            // First pass: estimate model (x,y) assuming z_data ≈ z_model
            // P_model ≈ R * (x_data, y_data, 0) + T  for x,y components
            const double x_model_approx = R[0][0]*x_data + R[0][1]*y_data + tx;
            const double y_model_approx = R[1][0]*x_data + R[1][1]*y_data + ty;

            // Convert to model pixel coordinates
            const double c_model = x_model_approx / model.xPixelSize;
            const double r_model = y_model_approx / model.yPixelSize;

            // Check bounds
            if (c_model < 0 || c_model >= model.cols - 1 ||
                r_model < 0 || r_model >= model.rows - 1)
                continue;

            // Bilinear interpolation to sample model z
            const uint32_t c0 = static_cast<uint32_t>(c_model);
            const uint32_t r0 = static_cast<uint32_t>(r_model);
            const uint32_t c1 = c0 + 1;
            const uint32_t r1 = r0 + 1;

            const float z00 = model.data[r0 * model.cols + c0];
            const float z01 = model.data[r0 * model.cols + c1];
            const float z10 = model.data[r1 * model.cols + c0];
            const float z11 = model.data[r1 * model.cols + c1];

            if (std::isnan(z00) || std::isnan(z01) || std::isnan(z10) || std::isnan(z11))
                continue;

            const double fc = c_model - c0;
            const double fr = r_model - r0;
            const double z_model = (1-fr) * ((1-fc) * z00 + fc * z01)
                                 + fr * ((1-fc) * z10 + fc * z11);

            // Now compute z_data from z_model using the inverse transform
            // z_model = R[2][0]*x_data + R[2][1]*y_data + R[2][2]*z_data + tz
            // z_data = (z_model - tz - R[2][0]*x_data - R[2][1]*y_data) / R[2][2]
            const double z_data = (z_model - tz - R[2][0]*x_data - R[2][1]*y_data) / R[2][2];

            // Add noise
            data.data[r * data.cols + c] = static_cast<float>(z_data) + noise(rng);
        }
    }

    return data;
}

void printUsage(const char* progName) {
    std::cerr << "Usage: " << progName << " <output_model.xv> <output_data.xv> [options]\n"
              << "\nGenerates synthetic test data for registration testing.\n"
              << "\nOptions:\n"
              << "  --size <n>        Image size in pixels (default: 256)\n"
              << "  --pixel <f>       Pixel size in mm (default: 0.1)\n"
              << "  --alpha <deg>     Rotation around Z axis (default: 3.0)\n"
              << "  --beta <deg>      Rotation around Y axis (default: 1.5)\n"
              << "  --gamma <deg>     Rotation around X axis (default: 2.0)\n"
              << "  --tx <mm>         Translation X (default: 5.0)\n"
              << "  --ty <mm>         Translation Y (default: -3.0)\n"
              << "  --tz <mm>         Translation Z (default: 0.5)\n"
              << "  --noise <mm>      Noise standard deviation (default: 0.05)\n"
              << "  --seed <n>        Random seed (default: 42)\n"
              << "\nExample:\n"
              << "  " << progName << " model.xv data.xv --alpha 5 --tx 10 --noise 0.1\n";
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        printUsage(argv[0]);
        return 1;
    }

    std::string modelPath = argv[1];
    std::string dataPath = argv[2];

    // Default parameters
    uint32_t size = 256;
    float pixelSize = 0.1f;
    float alpha = 3.0f;
    float beta = 1.5f;
    float gamma = 2.0f;
    float tx = 5.0f;
    float ty = -3.0f;
    float tz = 0.5f;
    float noise = 0.05f;
    unsigned seed = 42;

    // Parse options
    for (int i = 3; i < argc; i += 2) {
        if (i + 1 >= argc) {
            std::cerr << "Missing value for " << argv[i] << "\n";
            return 1;
        }
        std::string opt = argv[i];
        if (opt == "--size") size = static_cast<uint32_t>(std::stoi(argv[i+1]));
        else if (opt == "--pixel") pixelSize = std::stof(argv[i+1]);
        else if (opt == "--alpha") alpha = std::stof(argv[i+1]);
        else if (opt == "--beta") beta = std::stof(argv[i+1]);
        else if (opt == "--gamma") gamma = std::stof(argv[i+1]);
        else if (opt == "--tx") tx = std::stof(argv[i+1]);
        else if (opt == "--ty") ty = std::stof(argv[i+1]);
        else if (opt == "--tz") tz = std::stof(argv[i+1]);
        else if (opt == "--noise") noise = std::stof(argv[i+1]);
        else if (opt == "--seed") seed = static_cast<unsigned>(std::stoi(argv[i+1]));
        else {
            std::cerr << "Unknown option: " << opt << "\n";
            return 1;
        }
    }

    std::cout << "Generating synthetic test data...\n"
              << "  Size: " << size << "x" << size << " pixels\n"
              << "  Pixel size: " << pixelSize << " mm\n"
              << "  Transformation (data->model):\n"
              << "    alpha (Rz): " << alpha << " deg\n"
              << "    beta (Ry):  " << beta << " deg\n"
              << "    gamma (Rx): " << gamma << " deg\n"
              << "    tx: " << tx << " mm\n"
              << "    ty: " << ty << " mm\n"
              << "    tz: " << tz << " mm\n"
              << "  Noise: " << noise << " mm (std dev)\n"
              << "  Seed: " << seed << "\n";

    // Generate model (reference) surface
    std::cout << "Generating model surface...\n";
    ViffImage model = generateNonSymmetricSurface(size, size, pixelSize, 50.0f);

    // Generate data (transformed + noisy) surface
    std::cout << "Applying transformation and noise...\n";
    std::mt19937 rng(seed);
    ViffImage data = applyTransformation(model, alpha, beta, gamma, tx, ty, tz, rng, noise);

    // Save files
    ViffWriter writer;
    std::cout << "Saving model to: " << modelPath << "\n";
    if (!writer.save(modelPath, model)) {
        std::cerr << "Error saving model: " << writer.lastError() << "\n";
        return 1;
    }

    std::cout << "Saving data to: " << dataPath << "\n";
    if (!writer.save(dataPath, data)) {
        std::cerr << "Error saving data: " << writer.lastError() << "\n";
        return 1;
    }

    std::cout << "\nDone! To recover transformation, the expected result is:\n"
              << "  alpha = " << alpha << " deg\n"
              << "  beta  = " << beta << " deg\n"
              << "  gamma = " << gamma << " deg\n"
              << "  tx = " << tx << " mm\n"
              << "  ty = " << ty << " mm\n"
              << "  tz = " << tz << " mm\n";

    return 0;
}
