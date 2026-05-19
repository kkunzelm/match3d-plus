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

#include <cstdint>
#include <string>
#include <vector>

#pragma pack(push, 1)
struct ViffHeader {
    uint8_t  fileId;              // 0xAB = Khoros file
    uint8_t  fileType;            // 0x01 = VIFF
    uint8_t  release;
    uint8_t  version;
    uint8_t  machineDep;          // 0x02=Big-Endian, 0x08=Little-Endian
    uint8_t  padding[3];
    char     comment[512];
    uint32_t numberOfRows;        // In Khoros: number of data rows
    uint32_t numberOfColumns;     // In Khoros: number of data columns
    uint32_t lengthOfSubrow;
    int32_t  startX;
    int32_t  startY;
    float    xPixelSize;          // Metres per pixel in X
    float    yPixelSize;          // Metres per pixel in Y
    uint32_t locationType;        // 1=implicit, 2=explicit
    uint32_t locationDim;
    uint32_t numberOfImages;
    uint32_t numberOfBands;
    uint32_t dataStorageType;     // 5=float32
    uint32_t dataEncodingScheme;  // 0=uncompressed
    uint32_t mapScheme;
    uint32_t mapStorageType;
    uint32_t mapRowSize;
    uint32_t mapColumnSize;
    uint32_t mapSubrowSize;
    uint32_t mapEnable;
    uint32_t mapsPerCycle;
    uint32_t colorSpaceModel;
    uint32_t iSpare1;
    uint32_t iSpare2;
    float    fSpare1;             // User-defined (origin X)
    float    fSpare2;             // User-defined (origin Y)
    uint8_t  reserve[404];
};
#pragma pack(pop)
static_assert(sizeof(ViffHeader) == 1024, "ViffHeader must be exactly 1024 bytes");

// Depth image loaded from a VIFF/XV file.
// Khoros header stores width in numberOfRows and height in numberOfColumns (inverted names).
// ViffImage corrects for this: rows=height, cols=width, data[row*cols+col].
struct ViffImage {
    uint32_t rows = 0;        // Image height (= header.numberOfColumns)
    uint32_t cols = 0;        // Image width  (= header.numberOfRows)
    float    xPixelSize = 0;  // Metres per pixel
    float    yPixelSize = 0;
    float    originX = 0;     // FSpare1
    float    originY = 0;     // FSpare2
    std::vector<float> data;  // Row-major: data[row * cols + col]
    // Depth images use v=0 as "no scan" sentinel.
    // Difference images have negative/zero values that ARE valid — set this flag.
    bool     isDiffImage = false;

    float at(uint32_t row, uint32_t col) const { return data[row * cols + col]; }
    bool  isValid(uint32_t row, uint32_t col) const;
    uint32_t totalPixels() const { return rows * cols; }
};

class ViffReader {
public:
    // Load a VIFF/XV file into img.  Returns true on success.
    bool load(const std::string& path, ViffImage& img);
    const std::string& lastError() const { return error_; }

private:
    std::string error_;

    static bool systemIsLittleEndian();
    static void byteSwap4(void* p);
    static void fixHeader(ViffHeader& h, bool needSwap);
};
