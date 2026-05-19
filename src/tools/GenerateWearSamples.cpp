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

// Generate synthetic wear test samples for surface fitting
// 1. Flat plane with spherical depression (simulates flat sample wear)
// 2. Flat plane with truncated dome (simulates antagonist with wear facet)

#include "../io/ViffWriter.h"

#include <cmath>
#include <random>
#include <iostream>
#include <string>
#include <limits>

// Generate flat plane with spherical depression in center
// Simulates a flat wear specimen with material loss
ViffImage generatePlaneWithDepression(uint32_t rows, uint32_t cols,
                                       float pixelSize, float planeHeight,
                                       float sphereDiameterRatio,  // 0.2 = 20% of width
                                       float noiseStdDev) {
    ViffImage img;
    img.rows = rows;
    img.cols = cols;
    img.xPixelSize = pixelSize;
    img.yPixelSize = pixelSize;
    img.data.resize(static_cast<size_t>(rows) * cols);

    // Sphere parameters
    const float imageWidth = cols * pixelSize;
    const float imageHeight = rows * pixelSize;
    const float sphereDiameter = imageWidth * sphereDiameterRatio;
    const float sphereRadius = sphereDiameter / 2.0f;

    // Center of image (and sphere)
    const float cx = imageWidth / 2.0f;
    const float cy = imageHeight / 2.0f;

    // Sphere center is below the plane so the depression appears
    // The sphere surface is tangent at the plane edge of the depression
    // z_sphere = cz + sqrt(r² - (x-cx)² - (y-cy)²)
    // At center (x=cx, y=cy): z = cz + r = planeHeight - depth
    // Spherical cap depression geometry:
    // z = planeHeight - sqrt(r² - d²) where d = distance from center
    // At center (d=0): z = planeHeight - r (deepest point)
    // At edge (d=r): z = planeHeight - 0 = planeHeight (flush with plane)

    std::mt19937 rng(12345);
    std::normal_distribution<float> noise(0.0f, noiseStdDev);

    for (uint32_t r = 0; r < rows; ++r) {
        for (uint32_t c = 0; c < cols; ++c) {
            const float x = c * pixelSize;
            const float y = r * pixelSize;

            // Distance from center
            const float dx = x - cx;
            const float dy = y - cy;
            const float distSq = dx * dx + dy * dy;

            float z;
            if (distSq < sphereRadius * sphereRadius) {
                // Inside the depression: spherical cap going down
                const float dist = std::sqrt(distSq);
                const float zOffset = std::sqrt(sphereRadius * sphereRadius - distSq);
                z = planeHeight - zOffset;  // Depression (below plane)
            } else {
                // Outside: flat plane
                z = planeHeight;
            }

            // Add noise
            z += noise(rng);

            img.data[r * cols + c] = z;
        }
    }

    return img;
}

// Generate flat plane with truncated hemisphere (dome) on top
// Simulates a spherical antagonist with wear facet
ViffImage generatePlaneWithTruncatedDome(uint32_t rows, uint32_t cols,
                                          float pixelSize, float planeHeight,
                                          float sphereDiameterRatio,  // 0.2 = 20% of width
                                          float truncateRatio,        // 0.1 = clip top 10%
                                          float noiseStdDev) {
    ViffImage img;
    img.rows = rows;
    img.cols = cols;
    img.xPixelSize = pixelSize;
    img.yPixelSize = pixelSize;
    img.data.resize(static_cast<size_t>(rows) * cols);

    // Sphere parameters
    const float imageWidth = cols * pixelSize;
    const float imageHeight = rows * pixelSize;
    const float sphereDiameter = imageWidth * sphereDiameterRatio;
    const float sphereRadius = sphereDiameter / 2.0f;

    // Center of image (and dome base)
    const float cx = imageWidth / 2.0f;
    const float cy = imageHeight / 2.0f;

    // Truncation height: clip at (1 - truncateRatio) * radius
    // If truncateRatio = 0.1, we keep 90% of the dome height
    const float maxDomeHeight = sphereRadius * (1.0f - truncateRatio);

    std::mt19937 rng(67890);
    std::normal_distribution<float> noise(0.0f, noiseStdDev);

    for (uint32_t r = 0; r < rows; ++r) {
        for (uint32_t c = 0; c < cols; ++c) {
            const float x = c * pixelSize;
            const float y = r * pixelSize;

            // Distance from center
            const float dx = x - cx;
            const float dy = y - cy;
            const float distSq = dx * dx + dy * dy;

            float z;
            if (distSq < sphereRadius * sphereRadius) {
                // Inside the dome radius: hemisphere going up
                const float zOffset = std::sqrt(sphereRadius * sphereRadius - distSq);
                // Truncate at maxDomeHeight
                const float domeHeight = std::min(zOffset, maxDomeHeight);
                z = planeHeight + domeHeight;
            } else {
                // Outside: flat plane
                z = planeHeight;
            }

            // Add noise
            z += noise(rng);

            img.data[r * cols + c] = z;
        }
    }

    return img;
}

void printUsage(const char* progName) {
    std::cerr << "Usage: " << progName << " <type> <output.xv> [options]\n"
              << "\nTypes:\n"
              << "  depression   Flat plane with spherical depression (wear cavity)\n"
              << "  dome         Flat plane with truncated dome (antagonist wear facet)\n"
              << "\nOptions:\n"
              << "  --size <n>        Image size in pixels (default: 256)\n"
              << "  --pixel <f>       Pixel size in mm (default: 0.05)\n"
              << "  --height <f>      Plane height in mm (default: 1.0)\n"
              << "  --sphere <f>      Sphere diameter as fraction of width (default: 0.2)\n"
              << "  --truncate <f>    Truncation ratio for dome (default: 0.1)\n"
              << "  --noise <f>       Noise standard deviation in mm (default: 0.002)\n"
              << "\nExamples:\n"
              << "  " << progName << " depression wear_sample.xv --sphere 0.3\n"
              << "  " << progName << " dome antagonist.xv --truncate 0.15\n";
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        printUsage(argv[0]);
        return 1;
    }

    std::string type = argv[1];
    std::string outputPath = argv[2];

    // Default parameters
    uint32_t size = 256;
    float pixelSize = 0.05f;  // 50 µm pixels
    float planeHeight = 1.0f;  // 1 mm
    float sphereRatio = 0.2f;  // 20% of width
    float truncateRatio = 0.1f;  // 10% truncation
    float noise = 0.002f;  // 2 µm noise

    // Parse options
    for (int i = 3; i < argc; i += 2) {
        if (i + 1 >= argc) {
            std::cerr << "Missing value for " << argv[i] << "\n";
            return 1;
        }
        std::string opt = argv[i];
        if (opt == "--size") size = static_cast<uint32_t>(std::stoi(argv[i+1]));
        else if (opt == "--pixel") pixelSize = std::stof(argv[i+1]);
        else if (opt == "--height") planeHeight = std::stof(argv[i+1]);
        else if (opt == "--sphere") sphereRatio = std::stof(argv[i+1]);
        else if (opt == "--truncate") truncateRatio = std::stof(argv[i+1]);
        else if (opt == "--noise") noise = std::stof(argv[i+1]);
        else {
            std::cerr << "Unknown option: " << opt << "\n";
            return 1;
        }
    }

    ViffImage img;
    if (type == "depression") {
        std::cout << "Generating flat plane with spherical depression...\n";
        std::cout << "  Size: " << size << "x" << size << " pixels\n";
        std::cout << "  Pixel size: " << pixelSize << " mm\n";
        std::cout << "  Plane height: " << planeHeight << " mm\n";
        std::cout << "  Sphere diameter: " << (sphereRatio * 100) << "% of width = "
                  << (sphereRatio * size * pixelSize) << " mm\n";
        std::cout << "  Depression depth: " << (sphereRatio * size * pixelSize / 2) << " mm (at center)\n";
        std::cout << "  Noise: " << noise << " mm (std dev)\n";

        img = generatePlaneWithDepression(size, size, pixelSize, planeHeight, sphereRatio, noise);

    } else if (type == "dome") {
        std::cout << "Generating flat plane with truncated dome...\n";
        std::cout << "  Size: " << size << "x" << size << " pixels\n";
        std::cout << "  Pixel size: " << pixelSize << " mm\n";
        std::cout << "  Plane height: " << planeHeight << " mm\n";
        std::cout << "  Sphere diameter: " << (sphereRatio * 100) << "% of width = "
                  << (sphereRatio * size * pixelSize) << " mm\n";
        std::cout << "  Dome height (full): " << (sphereRatio * size * pixelSize / 2) << " mm\n";
        std::cout << "  Truncation: " << (truncateRatio * 100) << "% (top cut off)\n";
        std::cout << "  Dome height (truncated): " << (sphereRatio * size * pixelSize / 2 * (1.0f - truncateRatio)) << " mm\n";
        std::cout << "  Noise: " << noise << " mm (std dev)\n";

        img = generatePlaneWithTruncatedDome(size, size, pixelSize, planeHeight, sphereRatio, truncateRatio, noise);

    } else {
        std::cerr << "Unknown type: " << type << "\n";
        std::cerr << "Use 'depression' or 'dome'\n";
        return 1;
    }

    // Save file
    ViffWriter writer;
    std::cout << "Saving to: " << outputPath << "\n";
    if (!writer.save(outputPath, img)) {
        std::cerr << "Error saving file: " << writer.lastError() << "\n";
        return 1;
    }

    std::cout << "Done!\n";
    return 0;
}
