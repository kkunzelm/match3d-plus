#pragma once

/**
 * @file MeshProjection.h
 * @brief Project 3D mesh onto 2D heightmap for Match3D+ integration
 *
 * After interactive orientation in the 3D preview, the mesh is projected
 * onto the XY plane to create a 2.5D heightmap compatible with existing
 * Match3D+ processing (ROI, filters, ICP, statistics).
 *
 * Cross-platform compatible (Linux, Windows).
 */

#include "MeshData.h"
#include "io/ViffReader.h"
#include <Eigen/Core>

namespace mesh3d {

/**
 * @brief Parameters for mesh-to-heightmap projection
 */
struct ProjectionParams {
    double resolution = 0.025;  ///< Pixel size in mm (default 25 µm)
    bool autoSize = true;       ///< Compute image size from mesh bounds
    int width = 512;            ///< Image width if !autoSize
    int height = 512;           ///< Image height if !autoSize
    float noDataValue = 0.0f;   ///< Value for pixels with no mesh coverage

    /// Margin around mesh bounds (as fraction of dimensions)
    double margin = 0.02;       ///< 2% margin on each side
};

/**
 * @brief Result of projection operation
 */
struct ProjectionResult {
    ViffImage image;            ///< The projected heightmap
    int validPixels = 0;        ///< Number of pixels with mesh coverage
    int totalPixels = 0;        ///< Total image pixels
    double coveragePercent = 0; ///< Percentage of valid pixels

    /// Bounds of the projected area (in mm, after transformation)
    double xMin = 0, xMax = 0;
    double yMin = 0, yMax = 0;
    double zMin = 0, zMax = 0;
};

/**
 * @brief Project a transformed mesh onto the XY plane
 *
 * Algorithm:
 * 1. Apply transformation matrix to all vertices
 * 2. Compute XY bounding box of transformed mesh
 * 3. Create output grid with specified resolution
 * 4. For each triangle: rasterize and write Z values (Z-buffer, maximum wins)
 *
 * The result is a ViffImage compatible with all existing Match3D+ operations.
 *
 * @param mesh Input mesh data
 * @param transform 4x4 transformation matrix (from interactive orientation)
 * @param params Projection parameters
 * @return ProjectionResult containing the heightmap and statistics
 */
ProjectionResult projectToHeightmap(
    const MeshData& mesh,
    const Eigen::Matrix4d& transform,
    const ProjectionParams& params = {}
);

/**
 * @brief Compute bounding box of transformed mesh
 *
 * @param mesh Input mesh
 * @param transform Transformation matrix
 * @param outMin Output: minimum XYZ
 * @param outMax Output: maximum XYZ
 */
void computeTransformedBounds(
    const MeshData& mesh,
    const Eigen::Matrix4d& transform,
    std::array<double, 3>& outMin,
    std::array<double, 3>& outMax
);

/**
 * @brief Apply transformation to a single point
 *
 * @param point Input point (x, y, z)
 * @param transform 4x4 transformation matrix
 * @return Transformed point
 */
inline Eigen::Vector3d transformPoint(
    const Eigen::Vector3d& point,
    const Eigen::Matrix4d& transform
) {
    Eigen::Vector4d p(point.x(), point.y(), point.z(), 1.0);
    Eigen::Vector4d tp = transform * p;
    return Eigen::Vector3d(tp.x(), tp.y(), tp.z());
}

} // namespace mesh3d
