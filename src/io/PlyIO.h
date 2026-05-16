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
