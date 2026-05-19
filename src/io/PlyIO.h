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

#include "ViffReader.h"
#include <string>

// PLY import/export using happly (header-only).
class PlyIO {
public:
    // Probe a PLY file: read all vertices, estimate pixel spacing from point density.
    struct Probe {
        size_t  pointCount = 0;
        double  xMin = 0, xMax = 0, yMin = 0, yMax = 0;
        float   xPixelSize = 0;  // estimated
        float   yPixelSize = 0;
    };
    static bool probe(const std::string& path, Probe& out, std::string& error);

    // Rasterize a PLY point cloud onto a regular grid and return a ViffImage.
    // xPixelSize / yPixelSize must be > 0.
    static bool read(const std::string& path, ViffImage& img,
                     float xPixelSize, float yPixelSize,
                     std::string& error);

    // Write all valid pixels as PLY points (binary little-endian).
    static bool write(const std::string& path, const ViffImage& img,
                      std::string& error);
};
