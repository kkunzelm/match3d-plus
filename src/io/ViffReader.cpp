#include "ViffReader.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>

// ── ViffImage ─────────────────────────────────────────────────────────────────

bool ViffImage::isValid(uint32_t row, uint32_t col) const {
    float v = at(row, col);
    if (isDiffImage)
        return !std::isnan(v) && !std::isinf(v);
    return v > 0.0f && !std::isnan(v) && !std::isinf(v);
}

// ── ViffReader ────────────────────────────────────────────────────────────────

bool ViffReader::systemIsLittleEndian() {
    const uint16_t t = 1;
    return *reinterpret_cast<const uint8_t*>(&t) == 1;
}

void ViffReader::byteSwap4(void* p) {
    auto* b = static_cast<uint8_t*>(p);
    std::swap(b[0], b[3]);
    std::swap(b[1], b[2]);
}

void ViffReader::fixHeader(ViffHeader& h, bool needSwap) {
    if (!needSwap) return;
    byteSwap4(&h.numberOfRows);
    byteSwap4(&h.numberOfColumns);
    byteSwap4(&h.lengthOfSubrow);
    byteSwap4(&h.startX);
    byteSwap4(&h.startY);
    byteSwap4(&h.xPixelSize);
    byteSwap4(&h.yPixelSize);
    byteSwap4(&h.locationType);
    byteSwap4(&h.locationDim);
    byteSwap4(&h.numberOfImages);
    byteSwap4(&h.numberOfBands);
    byteSwap4(&h.dataStorageType);
    byteSwap4(&h.dataEncodingScheme);
    byteSwap4(&h.mapScheme);
    byteSwap4(&h.mapStorageType);
    byteSwap4(&h.mapRowSize);
    byteSwap4(&h.mapColumnSize);
    byteSwap4(&h.mapSubrowSize);
    byteSwap4(&h.mapEnable);
    byteSwap4(&h.mapsPerCycle);
    byteSwap4(&h.colorSpaceModel);
    byteSwap4(&h.iSpare1);
    byteSwap4(&h.iSpare2);
    byteSwap4(&h.fSpare1);
    byteSwap4(&h.fSpare2);
}

bool ViffReader::load(const std::string& path, ViffImage& img) {
    error_.clear();

    std::ifstream f(path, std::ios::binary);
    if (!f) {
        error_ = "Cannot open: " + path;
        return false;
    }

    ViffHeader h;
    f.read(reinterpret_cast<char*>(&h), sizeof(h));
    if (!f) {
        error_ = "Cannot read header (file too small)";
        return false;
    }

    if (h.fileId != 0xAB || h.fileType != 0x01) {
        error_ = "Not a VIFF file (wrong magic bytes)";
        return false;
    }

    // Determine if byte-swap is needed
    bool fileIsLittle = (h.machineDep == 0x08);
    bool sysIsLittle  = systemIsLittleEndian();
    bool needSwap     = (fileIsLittle != sysIsLittle);

    fixHeader(h, needSwap);

    if (h.dataStorageType != 0x05) {
        error_ = "Only float32 (type 5) is supported, got type "
                 + std::to_string(h.dataStorageType);
        return false;
    }
    if (h.dataEncodingScheme != 0x00) {
        error_ = "Compressed VIFF files are not supported";
        return false;
    }
    if (h.numberOfRows == 0 || h.numberOfColumns == 0) {
        error_ = "Invalid image dimensions (0x0)";
        return false;
    }

    // Khoros convention: numberOfRows = image WIDTH, numberOfColumns = image HEIGHT.
    img.cols       = h.numberOfRows;        // width (pixels per scanline)
    img.rows       = h.numberOfColumns;     // height (number of scanlines)
    img.xPixelSize = h.xPixelSize;
    img.yPixelSize = h.yPixelSize;
    img.originX    = h.fSpare1;
    img.originY    = h.fSpare2;

    const uint32_t bands = (h.numberOfBands > 0) ? h.numberOfBands : 1;
    const uint64_t n     = static_cast<uint64_t>(img.rows) * img.cols * bands;

    img.data.resize(n);
    f.read(reinterpret_cast<char*>(img.data.data()),
           static_cast<std::streamsize>(n * sizeof(float)));
    if (!f) {
        error_ = "Pixel data truncated";
        return false;
    }

    if (needSwap) {
        for (auto& v : img.data) byteSwap4(&v);
    }

    // Unit conversion for dental scan data:
    // VIFF stores pixel sizes in meters, but z values are typically in µm.
    // Convert everything to mm for practical use:
    // - Pixel sizes: meters → mm (multiply by 1000)
    // - Z values: µm → mm (divide by 1000)
    //
    // Heuristic: if pixel sizes are very small (< 0.001, i.e., < 1mm in meters)
    // and z values are large (> 1000), assume meters/µm convention.
    float zMin = 1e30f, zMax = -1e30f;
    for (const float v : img.data) {
        if (v > 0.0f && std::isfinite(v)) {
            zMin = std::min(zMin, v);
            zMax = std::max(zMax, v);
        }
    }

    const bool pixelSizeInMeters = (img.xPixelSize < 0.001f && img.xPixelSize > 0.0f);
    const bool zLikelyMicrons = (zMax > 1000.0f);  // z values > 1000 suggest µm

    if (pixelSizeInMeters && zLikelyMicrons) {
        // Convert pixel sizes from meters to mm
        img.xPixelSize *= 1000.0f;
        img.yPixelSize *= 1000.0f;
        img.originX *= 1000.0f;
        img.originY *= 1000.0f;

        // Convert z values from µm to mm
        for (auto& v : img.data) {
            if (v != 0.0f) v /= 1000.0f;
        }
    }

    // Auto-detect difference images: if any finite non-zero pixel is negative,
    // this cannot be a plain depth image (where zero means "no scan" and valid
    // pixels are always positive). Mark it so isValid() doesn't reject negatives.
    for (const float v : img.data) {
        if (v < 0.0f && std::isfinite(v)) {
            img.isDiffImage = true;
            break;
        }
    }

    return true;
}
