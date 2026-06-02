#include "STLReader.h"

#include <CGAL/Polygon_mesh_processing/polygon_soup_to_polygon_mesh.h>
#include <CGAL/Polygon_mesh_processing/repair_polygon_soup.h>
#include <CGAL/Polygon_mesh_processing/orient_polygon_soup.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <vector>

namespace mesh3d {

std::shared_ptr<MeshData> readSTL(const std::string& filePath, std::string& errorMsg)
{
    std::ifstream file(filePath, std::ios::binary);
    if (!file) {
        errorMsg = "Cannot open file: " + filePath;
        return nullptr;
    }

    // ── Read 80-byte header ──────────────────────────────────────────────────
    char rawHeader[81] = {};
    file.read(rawHeader, 80);
    std::string header(rawHeader);

    // Strip trailing nulls and whitespace
    while (!header.empty() && (header.back() == '\0' || header.back() == ' '))
        header.pop_back();

    // ── Read triangle count ──────────────────────────────────────────────────
    uint32_t nTriangles = 0;
    file.read(reinterpret_cast<char*>(&nTriangles), 4);
    if (!file || nTriangles == 0) {
        errorMsg = "Invalid or empty STL: " + filePath;
        return nullptr;
    }

    // ── Read all triangles into polygon soup ─────────────────────────────────
    std::vector<Point3>                     points;
    std::vector<std::array<std::size_t, 3>> faces;
    points.reserve(static_cast<std::size_t>(nTriangles) * 3);
    faces.reserve(nTriangles);

    std::size_t nCorrected = 0;

    for (uint32_t i = 0; i < nTriangles; ++i) {
        float buf[12]; // 3 floats normal + 9 floats vertices
        file.read(reinterpret_cast<char*>(buf), 48);
        uint16_t attr;
        file.read(reinterpret_cast<char*>(&attr), 2);

        if (!file) {
            errorMsg = "Unexpected end of file at triangle " + std::to_string(i);
            return nullptr;
        }

        // Use the STL-stored face normal to verify winding order.
        // Some scanners (e.g. Primescan) export triangles wound in the opposite
        // direction from others. We fix this per-face so orient_polygon_soup
        // starts from a consistently outward-facing soup.
        const float nx = buf[0], ny = buf[1], nz = buf[2];
        const float v0x = buf[3], v0y = buf[4], v0z = buf[5];
        const float v1x = buf[6], v1y = buf[7], v1z = buf[8];
        const float v2x = buf[9], v2y = buf[10], v2z = buf[11];

        std::size_t base = points.size();
        points.emplace_back(v0x, v0y, v0z);
        points.emplace_back(v1x, v1y, v1z);
        points.emplace_back(v2x, v2y, v2z);

        // Cross product of edges: (v1-v0) × (v2-v0)
        const float ex1 = v1x - v0x, ey1 = v1y - v0y, ez1 = v1z - v0z;
        const float ex2 = v2x - v0x, ey2 = v2y - v0y, ez2 = v2z - v0z;
        const float cx = ey1 * ez2 - ez1 * ey2;
        const float cy = ez1 * ex2 - ex1 * ez2;
        const float cz = ex1 * ey2 - ey1 * ex2;

        // Only correct when the stored normal is non-degenerate and anti-aligned.
        const float nMagSq = nx * nx + ny * ny + nz * nz;
        if (nMagSq > 1e-8f && (cx * nx + cy * ny + cz * nz) < 0.0f) {
            faces.push_back({base, base + 2, base + 1}); // swap v1 ↔ v2
            ++nCorrected;
        } else {
            faces.push_back({base, base + 1, base + 2});
        }
    }

    // If the majority of faces needed correction the entire mesh was inverted —
    // normal for some exporters, handled generically above.
    (void)nCorrected;

    // ── Repair and orient the polygon soup ───────────────────────────────────
    namespace PMP = CGAL::Polygon_mesh_processing;
    PMP::repair_polygon_soup(points, faces);
    PMP::orient_polygon_soup(points, faces);

    // ── Convert to SurfaceMesh ───────────────────────────────────────────────
    auto meshData = std::make_shared<MeshData>();
    PMP::polygon_soup_to_polygon_mesh(points, faces, meshData->mesh);

    if (meshData->mesh.is_empty()) {
        errorMsg = "Mesh is empty after conversion: " + filePath;
        return nullptr;
    }

    // ── Fill metadata ────────────────────────────────────────────────────────
    meshData->filePath = filePath;
    meshData->stlHeader = header;

    // Extract filename for display (cross-platform)
    std::filesystem::path fsPath(filePath);
    meshData->fileName = fsPath.filename().string();

    meshData->triangleCount = meshData->mesh.number_of_faces();
    meshData->vertexCount = meshData->mesh.number_of_vertices();

    // ── Compute bounding box ─────────────────────────────────────────────────
    bool first = true;
    for (auto v : meshData->mesh.vertices()) {
        const Point3& p = meshData->mesh.point(v);
        if (first) {
            meshData->boundsMin = {p.x(), p.y(), p.z()};
            meshData->boundsMax = {p.x(), p.y(), p.z()};
            first = false;
        } else {
            meshData->boundsMin[0] = std::min(meshData->boundsMin[0], p.x());
            meshData->boundsMin[1] = std::min(meshData->boundsMin[1], p.y());
            meshData->boundsMin[2] = std::min(meshData->boundsMin[2], p.z());
            meshData->boundsMax[0] = std::max(meshData->boundsMax[0], p.x());
            meshData->boundsMax[1] = std::max(meshData->boundsMax[1], p.y());
            meshData->boundsMax[2] = std::max(meshData->boundsMax[2], p.z());
        }
    }

    return meshData;
}

} // namespace mesh3d
