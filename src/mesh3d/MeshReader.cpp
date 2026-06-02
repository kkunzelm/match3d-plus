#include "MeshReader.h"
#include "STLReader.h"

#include <CGAL/IO/PLY.h>
#include <CGAL/IO/OBJ.h>
#include <CGAL/Polygon_mesh_processing/orient_polygon_soup.h>
#include <CGAL/Polygon_mesh_processing/polygon_soup_to_polygon_mesh.h>
#include <CGAL/Polygon_mesh_processing/repair_polygon_soup.h>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>

namespace mesh3d {

namespace {

// Helper: get lowercase file extension
std::string getLowerExtension(const std::string& filePath) {
    std::filesystem::path p(filePath);
    std::string ext = p.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return ext;
}

// Helper: center mesh at origin and compute bounds
void centerAndComputeBounds(std::shared_ptr<MeshData>& meshData) {
    if (!meshData || meshData->mesh.is_empty()) return;

    // Compute bounding box
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

    // Center at origin
    double cx = (meshData->boundsMin[0] + meshData->boundsMax[0]) / 2.0;
    double cy = (meshData->boundsMin[1] + meshData->boundsMax[1]) / 2.0;
    double cz = (meshData->boundsMin[2] + meshData->boundsMax[2]) / 2.0;

    for (auto v : meshData->mesh.vertices()) {
        Point3& p = meshData->mesh.point(v);
        p = Point3(p.x() - cx, p.y() - cy, p.z() - cz);
    }

    // Update bounds to reflect centered mesh
    meshData->boundsMin[0] -= cx;
    meshData->boundsMin[1] -= cy;
    meshData->boundsMin[2] -= cz;
    meshData->boundsMax[0] -= cx;
    meshData->boundsMax[1] -= cy;
    meshData->boundsMax[2] -= cz;
}

} // anonymous namespace

MeshFormat detectFormat(const std::string& filePath) {
    std::string ext = getLowerExtension(filePath);
    if (ext == ".stl") return MeshFormat::STL;
    if (ext == ".ply") return MeshFormat::PLY;
    if (ext == ".obj") return MeshFormat::OBJ;
    return MeshFormat::Unknown;
}

std::shared_ptr<MeshData> readMesh(const std::string& filePath, std::string& errorMsg) {
    MeshFormat format = detectFormat(filePath);

    switch (format) {
    case MeshFormat::STL:
        return readSTL(filePath, errorMsg);
    case MeshFormat::PLY:
        return readPLY(filePath, errorMsg);
    case MeshFormat::OBJ:
        return readOBJ(filePath, errorMsg);
    default:
        errorMsg = "Unknown file format: " + filePath;
        return nullptr;
    }
}

std::shared_ptr<MeshData> readPLY(const std::string& filePath, std::string& errorMsg) {
    auto meshData = std::make_shared<MeshData>();

    // Try to read directly as a polygon mesh first
    std::ifstream input(filePath);
    if (!input) {
        errorMsg = "Cannot open file: " + filePath;
        return nullptr;
    }

    if (!CGAL::IO::read_PLY(input, meshData->mesh)) {
        // If direct read fails, try polygon soup approach
        input.close();
        input.open(filePath);

        std::vector<Point3> points;
        std::vector<std::vector<std::size_t>> polygons;

        if (!CGAL::IO::read_PLY(input, points, polygons)) {
            errorMsg = "Failed to read PLY file: " + filePath;
            return nullptr;
        }

        if (polygons.empty()) {
            errorMsg = "PLY file has no faces (use regular PLY import for point clouds): " + filePath;
            return nullptr;
        }

        // Repair and orient the polygon soup
        namespace PMP = CGAL::Polygon_mesh_processing;
        PMP::repair_polygon_soup(points, polygons);
        PMP::orient_polygon_soup(points, polygons);

        // Convert to surface mesh
        PMP::polygon_soup_to_polygon_mesh(points, polygons, meshData->mesh);
    }

    if (meshData->mesh.is_empty()) {
        errorMsg = "PLY mesh is empty or failed to convert: " + filePath;
        return nullptr;
    }

    // Fill metadata
    meshData->filePath = filePath;
    std::filesystem::path fsPath(filePath);
    meshData->fileName = fsPath.filename().string();
    meshData->triangleCount = meshData->mesh.number_of_faces();
    meshData->vertexCount = meshData->mesh.number_of_vertices();

    // Center mesh and compute bounds
    centerAndComputeBounds(meshData);

    return meshData;
}

std::shared_ptr<MeshData> readOBJ(const std::string& filePath, std::string& errorMsg) {
    auto meshData = std::make_shared<MeshData>();

    std::ifstream input(filePath);
    if (!input) {
        errorMsg = "Cannot open file: " + filePath;
        return nullptr;
    }

    if (!CGAL::IO::read_OBJ(input, meshData->mesh)) {
        // If direct read fails, try polygon soup approach
        input.close();
        input.open(filePath);

        std::vector<Point3> points;
        std::vector<std::vector<std::size_t>> polygons;

        if (!CGAL::IO::read_OBJ(input, points, polygons)) {
            errorMsg = "Failed to read OBJ file: " + filePath;
            return nullptr;
        }

        if (polygons.empty()) {
            errorMsg = "OBJ file has no faces: " + filePath;
            return nullptr;
        }

        // Repair and orient the polygon soup
        namespace PMP = CGAL::Polygon_mesh_processing;
        PMP::repair_polygon_soup(points, polygons);
        PMP::orient_polygon_soup(points, polygons);

        // Convert to surface mesh
        PMP::polygon_soup_to_polygon_mesh(points, polygons, meshData->mesh);
    }

    if (meshData->mesh.is_empty()) {
        errorMsg = "OBJ mesh is empty or failed to convert: " + filePath;
        return nullptr;
    }

    // Fill metadata
    meshData->filePath = filePath;
    std::filesystem::path fsPath(filePath);
    meshData->fileName = fsPath.filename().string();
    meshData->triangleCount = meshData->mesh.number_of_faces();
    meshData->vertexCount = meshData->mesh.number_of_vertices();

    // Center mesh and compute bounds
    centerAndComputeBounds(meshData);

    return meshData;
}

std::string getMeshFileFilter() {
    return "3D Mesh Files (*.stl *.ply *.obj);;STL Files (*.stl);;PLY Files (*.ply);;OBJ Files (*.obj);;All Files (*)";
}

} // namespace mesh3d
