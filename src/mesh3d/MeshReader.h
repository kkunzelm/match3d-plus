#pragma once

/**
 * @file MeshReader.h
 * @brief Unified 3D mesh file reader for Match3D+
 *
 * Reads various 3D mesh formats into CGAL SurfaceMesh format:
 * - STL (binary)
 * - PLY (with faces)
 * - OBJ
 *
 * All meshes are auto-centered at origin for intuitive rotation.
 * Cross-platform compatible (Linux, Windows).
 */

#include "MeshData.h"
#include <memory>
#include <string>

namespace mesh3d {

/// Supported mesh file formats
enum class MeshFormat {
    Unknown,
    STL,
    PLY,
    OBJ
};

/**
 * @brief Detect mesh format from file extension
 * @param filePath Path to mesh file
 * @return Detected format, or Unknown if not recognized
 */
MeshFormat detectFormat(const std::string& filePath);

/**
 * @brief Read a mesh file (auto-detect format)
 *
 * Reads STL, PLY, or OBJ files based on extension.
 * The mesh is automatically centered at the origin.
 *
 * @param filePath Full path to the mesh file
 * @param errorMsg Output: error message if reading fails
 * @return Shared pointer to MeshData, or nullptr on error
 */
std::shared_ptr<MeshData> readMesh(const std::string& filePath, std::string& errorMsg);

/**
 * @brief Read a PLY mesh file
 *
 * Reads PLY files with face data into CGAL SurfaceMesh.
 * Note: This is different from PlyIO which rasterizes point clouds.
 *
 * @param filePath Full path to the PLY file
 * @param errorMsg Output: error message if reading fails
 * @return Shared pointer to MeshData, or nullptr on error
 */
std::shared_ptr<MeshData> readPLY(const std::string& filePath, std::string& errorMsg);

/**
 * @brief Read an OBJ mesh file
 *
 * @param filePath Full path to the OBJ file
 * @param errorMsg Output: error message if reading fails
 * @return Shared pointer to MeshData, or nullptr on error
 */
std::shared_ptr<MeshData> readOBJ(const std::string& filePath, std::string& errorMsg);

/**
 * @brief Get file filter string for open dialogs
 * @return Filter string like "3D Mesh Files (*.stl *.ply *.obj)"
 */
std::string getMeshFileFilter();

} // namespace mesh3d
