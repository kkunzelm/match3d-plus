#include "MeshProjection.h"

#include <cmath>
#include <limits>

namespace mesh3d {

void computeTransformedBounds(
    const MeshData& mesh,
    const Eigen::Matrix4d& transform,
    std::array<double, 3>& outMin,
    std::array<double, 3>& outMax)
{
    bool first = true;

    for (auto v : mesh.mesh.vertices()) {
        const Point3& p = mesh.mesh.point(v);
        Eigen::Vector3d pt(p.x(), p.y(), p.z());
        Eigen::Vector3d tp = transformPoint(pt, transform);

        if (first) {
            outMin = {tp.x(), tp.y(), tp.z()};
            outMax = {tp.x(), tp.y(), tp.z()};
            first = false;
        } else {
            outMin[0] = std::min(outMin[0], tp.x());
            outMin[1] = std::min(outMin[1], tp.y());
            outMin[2] = std::min(outMin[2], tp.z());
            outMax[0] = std::max(outMax[0], tp.x());
            outMax[1] = std::max(outMax[1], tp.y());
            outMax[2] = std::max(outMax[2], tp.z());
        }
    }
}

namespace {

// Barycentric rasterization helpers

struct Triangle2D {
    double x0, y0, z0;
    double x1, y1, z1;
    double x2, y2, z2;

    // Bounding box
    double minX, maxX, minY, maxY;

    void computeBounds() {
        minX = std::min({x0, x1, x2});
        maxX = std::max({x0, x1, x2});
        minY = std::min({y0, y1, y2});
        maxY = std::max({y0, y1, y2});
    }

    // Signed area * 2 (for barycentric coordinates)
    double signedArea2() const {
        return (x1 - x0) * (y2 - y0) - (x2 - x0) * (y1 - y0);
    }

    // Compute barycentric coordinates for point (px, py)
    // Returns false if point is outside triangle
    bool barycentric(double px, double py, double& u, double& v, double& w) const {
        double area2 = signedArea2();
        if (std::abs(area2) < 1e-12) return false;  // Degenerate triangle

        double invArea2 = 1.0 / area2;

        // Barycentric coordinates
        u = ((x1 - px) * (y2 - py) - (x2 - px) * (y1 - py)) * invArea2;
        v = ((x2 - px) * (y0 - py) - (x0 - px) * (y2 - py)) * invArea2;
        w = 1.0 - u - v;

        // Check if inside triangle (with small tolerance for edge cases)
        const double eps = -1e-6;
        return (u >= eps && v >= eps && w >= eps);
    }

    // Interpolate Z at point (px, py) using barycentric coordinates
    double interpolateZ(double u, double v, double w) const {
        return u * z0 + v * z1 + w * z2;
    }
};

} // anonymous namespace

ProjectionResult projectToHeightmap(
    const MeshData& mesh,
    const Eigen::Matrix4d& transform,
    const ProjectionParams& params)
{
    ProjectionResult result;

    // ── Compute transformed bounds ───────────────────────────────────────────
    std::array<double, 3> tMin, tMax;
    computeTransformedBounds(mesh, transform, tMin, tMax);

    result.xMin = tMin[0];
    result.xMax = tMax[0];
    result.yMin = tMin[1];
    result.yMax = tMax[1];
    result.zMin = tMin[2];
    result.zMax = tMax[2];

    // ── Compute image dimensions ─────────────────────────────────────────────
    double width_mm = tMax[0] - tMin[0];
    double height_mm = tMax[1] - tMin[1];

    // Add margin
    double marginX = width_mm * params.margin;
    double marginY = height_mm * params.margin;
    tMin[0] -= marginX;
    tMax[0] += marginX;
    tMin[1] -= marginY;
    tMax[1] += marginY;
    width_mm = tMax[0] - tMin[0];
    height_mm = tMax[1] - tMin[1];

    int width, height;
    if (params.autoSize) {
        width = static_cast<int>(std::ceil(width_mm / params.resolution)) + 1;
        height = static_cast<int>(std::ceil(height_mm / params.resolution)) + 1;
        // Clamp to reasonable limits
        width = std::clamp(width, 1, 8192);
        height = std::clamp(height, 1, 8192);
    } else {
        width = params.width;
        height = params.height;
    }

    result.totalPixels = width * height;

    // ── Initialize Z-buffer ──────────────────────────────────────────────────
    // Using -infinity to track "no data yet"
    std::vector<float> zBuffer(static_cast<size_t>(width) * height,
                                -std::numeric_limits<float>::infinity());

    // ── Rasterize each triangle ──────────────────────────────────────────────
    for (auto face : mesh.mesh.faces()) {
        // Get face vertices
        auto h = mesh.mesh.halfedge(face);
        auto v0 = mesh.mesh.target(h);
        auto v1 = mesh.mesh.target(mesh.mesh.next(h));
        auto v2 = mesh.mesh.target(mesh.mesh.next(mesh.mesh.next(h)));

        const Point3& p0 = mesh.mesh.point(v0);
        const Point3& p1 = mesh.mesh.point(v1);
        const Point3& p2 = mesh.mesh.point(v2);

        // Transform vertices
        Eigen::Vector3d tp0 = transformPoint(Eigen::Vector3d(p0.x(), p0.y(), p0.z()), transform);
        Eigen::Vector3d tp1 = transformPoint(Eigen::Vector3d(p1.x(), p1.y(), p1.z()), transform);
        Eigen::Vector3d tp2 = transformPoint(Eigen::Vector3d(p2.x(), p2.y(), p2.z()), transform);

        // Create 2D triangle for rasterization
        Triangle2D tri;
        tri.x0 = tp0.x(); tri.y0 = tp0.y(); tri.z0 = tp0.z();
        tri.x1 = tp1.x(); tri.y1 = tp1.y(); tri.z1 = tp1.z();
        tri.x2 = tp2.x(); tri.y2 = tp2.y(); tri.z2 = tp2.z();
        tri.computeBounds();

        // Skip degenerate triangles
        if (std::abs(tri.signedArea2()) < 1e-12) continue;

        // Convert bounding box to pixel coordinates
        int pixMinX = static_cast<int>((tri.minX - tMin[0]) / params.resolution);
        int pixMaxX = static_cast<int>((tri.maxX - tMin[0]) / params.resolution) + 1;
        int pixMinY = static_cast<int>((tri.minY - tMin[1]) / params.resolution);
        int pixMaxY = static_cast<int>((tri.maxY - tMin[1]) / params.resolution) + 1;

        // Clamp to image bounds
        pixMinX = std::clamp(pixMinX, 0, width - 1);
        pixMaxX = std::clamp(pixMaxX, 0, width - 1);
        pixMinY = std::clamp(pixMinY, 0, height - 1);
        pixMaxY = std::clamp(pixMaxY, 0, height - 1);

        // Rasterize
        for (int py = pixMinY; py <= pixMaxY; ++py) {
            for (int px = pixMinX; px <= pixMaxX; ++px) {
                // Pixel center in world coordinates
                double wx = tMin[0] + (px + 0.5) * params.resolution;
                double wy = tMin[1] + (py + 0.5) * params.resolution;

                double u, v, w;
                if (tri.barycentric(wx, wy, u, v, w)) {
                    double z = tri.interpolateZ(u, v, w);
                    size_t idx = static_cast<size_t>(py) * width + px;

                    // Z-buffer: keep maximum Z (surface closest to viewer from +Z)
                    if (z > zBuffer[idx]) {
                        zBuffer[idx] = static_cast<float>(z);
                    }
                }
            }
        }
    }

    // ── Create ViffImage ─────────────────────────────────────────────────────
    ViffImage& img = result.image;
    img.cols = static_cast<uint32_t>(width);   // ViffImage uses cols for width
    img.rows = static_cast<uint32_t>(height);  // ViffImage uses rows for height
    img.xPixelSize = static_cast<float>(params.resolution);
    img.yPixelSize = static_cast<float>(params.resolution);
    img.data.resize(static_cast<size_t>(width) * height);

    int validCount = 0;
    for (size_t i = 0; i < zBuffer.size(); ++i) {
        if (zBuffer[i] == -std::numeric_limits<float>::infinity()) {
            img.data[i] = params.noDataValue;
        } else {
            img.data[i] = zBuffer[i];
            ++validCount;
        }
    }

    result.validPixels = validCount;
    result.coveragePercent = 100.0 * validCount / result.totalPixels;

    return result;
}

} // namespace mesh3d
