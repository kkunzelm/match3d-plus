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

#include "ViffReader.h"  // for ViffHeader and ViffImage
#include <string>

class ViffWriter {
public:
    // Write a ViffImage to path. Always writes Little-Endian (x86 native).
    bool save(const std::string& path, const ViffImage& img);

    // Convenience: write raw float array with explicit dimensions/pixel sizes.
    bool save(const std::string& path,
              uint32_t rows, uint32_t cols,
              const float* data,
              float xPixelSize = 0.0f, float yPixelSize = 0.0f,
              float originX = 0.0f, float originY = 0.0f);

    const std::string& lastError() const { return error_; }

private:
    std::string error_;

    static ViffHeader makeHeader(uint32_t rows, uint32_t cols,
                                 float xPixelSize, float yPixelSize,
                                 float originX, float originY);
};
