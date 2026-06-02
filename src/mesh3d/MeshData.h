#pragma once

/**
 * @file MeshData.h
 * @brief Data structures for 3D mesh handling in Match3D+
 *
 * This module provides CGAL-based mesh data structures for STL import.
 * Cross-platform compatible (Linux, Windows).
 */

#include <CGAL/Exact_predicates_inexact_constructions_kernel.h>
#include <CGAL/Surface_mesh.h>
#include <Eigen/Core>
#include <array>
#include <string>

namespace mesh3d {

// ── CGAL Type Aliases ────────────────────────────────────────────────────────
using Kernel       = CGAL::Exact_predicates_inexact_constructions_kernel;
using Point3       = Kernel::Point_3;
using Vector3K     = Kernel::Vector_3;
using SurfaceMesh  = CGAL::Surface_mesh<Point3>;
using VertexDesc   = SurfaceMesh::Vertex_index;
using FaceDesc     = SurfaceMesh::Face_index;
using HalfedgeDesc = SurfaceMesh::Halfedge_index;

/**
 * @brief Container for imported STL mesh data
 *
 * Stores the mesh geometry along with metadata and transformation state.
 * The transform matrix is updated interactively by the user during import.
 */
struct MeshData {
    // ── Identity ─────────────────────────────────────────────────────────────
    std::string filePath;      ///< Full path to source STL file
    std::string fileName;      ///< Base filename (for display)
    std::string stlHeader;     ///< Raw 80-byte STL header text

    // ── Geometry ─────────────────────────────────────────────────────────────
    SurfaceMesh mesh;          ///< CGAL surface mesh

    // ── Bounds (computed after import, before transformation) ────────────────
    std::array<double, 3> boundsMin{};  ///< Minimum XYZ coordinates
    std::array<double, 3> boundsMax{};  ///< Maximum XYZ coordinates
    std::size_t triangleCount = 0;      ///< Number of faces
    std::size_t vertexCount   = 0;      ///< Number of vertices

    // ── Interactive Transformation ───────────────────────────────────────────
    /// Current transformation matrix (updated during interactive orientation)
    /// Applied to mesh before projection to 2D heightmap
    Eigen::Matrix4d transform = Eigen::Matrix4d::Identity();

    // ── Helper Methods ───────────────────────────────────────────────────────

    /// Compute bounding box dimensions (max - min)
    std::array<double, 3> dimensions() const {
        return {
            boundsMax[0] - boundsMin[0],
            boundsMax[1] - boundsMin[1],
            boundsMax[2] - boundsMin[2]
        };
    }

    /// Get center of bounding box
    std::array<double, 3> center() const {
        return {
            (boundsMin[0] + boundsMax[0]) / 2.0,
            (boundsMin[1] + boundsMax[1]) / 2.0,
            (boundsMin[2] + boundsMax[2]) / 2.0
        };
    }

    /// Check if mesh is valid (has vertices and faces)
    bool isValid() const {
        return vertexCount > 0 && triangleCount > 0;
    }
};

} // namespace mesh3d
