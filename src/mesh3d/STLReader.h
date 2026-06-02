#pragma once

/**
 * @file STLReader.h
 * @brief Binary STL file reader for Match3D+
 *
 * Reads binary STL files into CGAL SurfaceMesh format.
 * Includes automatic winding correction for different scanner exports.
 * Cross-platform compatible (Linux, Windows).
 */

#include "MeshData.h"
#include <memory>
#include <string>

namespace mesh3d {

/**
 * @brief Read a binary STL file into MeshData
 *
 * Features:
 * - Reads standard binary STL format (80-byte header + triangles)
 * - Automatic winding order correction (handles Primescan-style exports)
 * - CGAL polygon soup repair and orientation
 * - Computes bounding box
 *
 * @param filePath Full path to the STL file
 * @param errorMsg Output: error message if reading fails
 * @return Shared pointer to MeshData, or nullptr on error
 *
 * @note Only binary STL is supported; ASCII STL will fail.
 */
std::shared_ptr<MeshData> readSTL(const std::string& filePath, std::string& errorMsg);

} // namespace mesh3d
