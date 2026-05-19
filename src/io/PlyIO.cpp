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

#include "PlyIO.h"

#include <happly.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>

// ── Probe ────────────────────────────────────────────────────────────────────

bool PlyIO::probe(const std::string& path, Probe& out, std::string& error) {
    try {
        happly::PLYData ply(path);
        if (!ply.hasElement("vertex")) {
            error = "PLY file has no 'vertex' element";
            return false;
        }
        // getProperty<double> auto-promotes from float
        auto xs = ply.getElement("vertex").getProperty<double>("x");
        auto ys = ply.getElement("vertex").getProperty<double>("y");

        out.pointCount = xs.size();
        if (out.pointCount == 0) {
            error = "PLY file contains no vertices";
            return false;
        }

        out.xMin = *std::min_element(xs.begin(), xs.end());
        out.xMax = *std::max_element(xs.begin(), xs.end());
        out.yMin = *std::min_element(ys.begin(), ys.end());
        out.yMax = *std::max_element(ys.begin(), ys.end());

        // Estimate pixel spacing from point density
        const double area = (out.xMax - out.xMin) * (out.yMax - out.yMin);
        const double ps = (area > 0.0 && out.pointCount > 1)
                          ? std::sqrt(area / static_cast<double>(out.pointCount))
                          : 1.0;

        // Refine with 1st-percentile of x-differences for X spacing
        // (handles regular grids: all diffs are multiples of pixel size)
        auto refine = [](std::vector<double>& vals) -> double {
            std::sort(vals.begin(), vals.end());
            std::vector<double> diffs;
            diffs.reserve(vals.size());
            for (size_t i = 1; i < vals.size(); ++i) {
                double d = vals[i] - vals[i - 1];
                if (d > 1e-12) diffs.push_back(d);
            }
            if (diffs.empty()) return 0.0;
            std::sort(diffs.begin(), diffs.end());
            // 1st percentile → smallest common step (the pixel spacing)
            return diffs[std::max(size_t(0), diffs.size() / 100)];
        };

        // Sample at most 20000 values for the percentile estimate
        auto sample = [](const std::vector<double>& src) {
            if (src.size() <= 20000) return src;
            std::vector<double> s;
            s.reserve(20000);
            const size_t step = src.size() / 20000;
            for (size_t i = 0; i < src.size(); i += step) s.push_back(src[i]);
            return s;
        };

        auto xsamp = sample(xs);
        auto ysamp = sample(ys);

        double xr = refine(xsamp);
        double yr = refine(ysamp);

        out.xPixelSize = static_cast<float>(xr > 1e-12 ? xr : ps);
        out.yPixelSize = static_cast<float>(yr > 1e-12 ? yr : ps);

    } catch (const std::exception& e) {
        error = e.what();
        return false;
    }
    return true;
}

// ── Read (rasterize) ─────────────────────────────────────────────────────────

bool PlyIO::read(const std::string& path, ViffImage& img,
                 float xPixelSize, float yPixelSize,
                 std::string& error) {
    if (xPixelSize <= 0 || yPixelSize <= 0) {
        error = "Pixel size must be > 0";
        return false;
    }

    try {
        happly::PLYData ply(path);
        if (!ply.hasElement("vertex")) {
            error = "PLY file has no 'vertex' element";
            return false;
        }
        auto xs = ply.getElement("vertex").getProperty<double>("x");
        auto ys = ply.getElement("vertex").getProperty<double>("y");
        auto zs = ply.getElement("vertex").getProperty<double>("z");

        const size_t N = xs.size();
        if (N == 0) { error = "No vertices"; return false; }

        const double xMin = *std::min_element(xs.begin(), xs.end());
        const double yMin = *std::min_element(ys.begin(), ys.end());
        const double xMax = *std::max_element(xs.begin(), xs.end());
        const double yMax = *std::max_element(ys.begin(), ys.end());

        const uint32_t cols = static_cast<uint32_t>((xMax - xMin) / xPixelSize + 1.5);
        const uint32_t rows = static_cast<uint32_t>((yMax - yMin) / yPixelSize + 1.5);

        if (cols == 0 || rows == 0 || cols > 100000 || rows > 100000) {
            error = "Implausible grid dimensions (" + std::to_string(cols) + " × "
                    + std::to_string(rows) + ") — check pixel size";
            return false;
        }

        // Accumulate z values per cell (average multiple hits)
        const size_t total = static_cast<size_t>(rows) * cols;
        std::vector<double>   sumZ(total, 0.0);
        std::vector<uint32_t> cnt(total, 0);

        // Y is flipped so row 0 = yMax (top of image = largest Y in world space),
        // matching the standard "view from above" orientation where Y increases upward.
        for (size_t i = 0; i < N; ++i) {
            const int col = static_cast<int>((xs[i] - xMin) / xPixelSize + 0.5);
            const int row = static_cast<int>((yMax - ys[i]) / yPixelSize + 0.5);
            if (col < 0 || col >= static_cast<int>(cols) ||
                row < 0 || row >= static_cast<int>(rows)) continue;
            const size_t idx = static_cast<size_t>(row) * cols + col;
            sumZ[idx] += zs[i];
            cnt[idx]++;
        }

        img.rows       = rows;
        img.cols       = cols;
        img.xPixelSize = xPixelSize;
        img.yPixelSize = yPixelSize;
        img.originX    = static_cast<float>(xMin);
        img.originY    = static_cast<float>(yMax);  // row 0 corresponds to yMax
        img.data.assign(total, 0.0f);

        for (size_t i = 0; i < total; ++i)
            if (cnt[i] > 0)
                img.data[i] = static_cast<float>(sumZ[i] / cnt[i]);

    } catch (const std::exception& e) {
        error = e.what();
        return false;
    }
    return true;
}

// ── Write ─────────────────────────────────────────────────────────────────────

bool PlyIO::write(const std::string& path, const ViffImage& img, std::string& error) {
    try {
        std::vector<std::array<double, 3>> pts;
        pts.reserve(img.rows * img.cols / 2);  // rough pre-alloc

        // originX = world X of column 0; originY = world Y of row 0 (= yMax, Y decreases downward).
        for (uint32_t r = 0; r < img.rows; ++r) {
            for (uint32_t c = 0; c < img.cols; ++c) {
                if (!img.isValid(r, c)) continue;
                pts.push_back({
                    img.originX + c * static_cast<double>(img.xPixelSize),
                    img.originY - r * static_cast<double>(img.yPixelSize),
                    static_cast<double>(img.at(r, c))
                });
            }
        }

        happly::PLYData ply;
        ply.addVertexPositions(pts);
        ply.write(path, happly::DataFormat::Binary);

    } catch (const std::exception& e) {
        error = e.what();
        return false;
    }
    return true;
}
