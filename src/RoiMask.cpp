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

#include "RoiMask.h"
#include "io/ViffReader.h"

#include <QFile>
#include <QTextStream>
#include <algorithm>
#include <cmath>
#include <numbers>

namespace {
    constexpr float kDegToRadF = static_cast<float>(std::numbers::pi) / 180.0f;
}

// ── Construction ──────────────────────────────────────────────────────────────

RoiMask::RoiMask(uint32_t rows, uint32_t cols, bool initiallySelected)
    : rows_(rows), cols_(cols), mask_(rows * cols, initiallySelected)
{}

bool RoiMask::isSelected(uint32_t row, uint32_t col) const {
    if (mask_.empty()) return true;  // no mask = everything selected
    return mask_[row * cols_ + col];
}

void RoiMask::set(uint32_t row, uint32_t col, bool sel) {
    mask_[row * cols_ + col] = sel;
}

// ── Bulk operations ───────────────────────────────────────────────────────────

void RoiMask::selectAll()   { std::fill(mask_.begin(), mask_.end(), true);  }
void RoiMask::unselectAll() { std::fill(mask_.begin(), mask_.end(), false); }
void RoiMask::invert() {
    for (size_t i = 0; i < mask_.size(); ++i)
        mask_[i] = !mask_[i];
}

// ── Shape selection ───────────────────────────────────────────────────────────

bool RoiMask::pointInPolygon(const QPolygonF& poly, float col, float row) {
    const int n = poly.size();
    bool inside = false;
    for (int i = 0, j = n - 1; i < n; j = i++) {
        const float xi = static_cast<float>(poly[i].x()), yi = static_cast<float>(poly[i].y());
        const float xj = static_cast<float>(poly[j].x()), yj = static_cast<float>(poly[j].y());
        if (((yi > row) != (yj > row)) &&
            (col < (xj - xi) * (row - yi) / (yj - yi) + xi))
            inside = !inside;
    }
    return inside;
}

void RoiMask::applyPolygon(const QPolygonF& poly, bool select) {
    if (poly.size() < 3 || mask_.empty()) return;

    // Bounding box for fast rejection
    const QRectF bb = poly.boundingRect();
    const int r0 = std::max(0,              static_cast<int>(std::floor(bb.top())));
    const int r1 = std::min((int)rows_ - 1, static_cast<int>(std::ceil(bb.bottom())));
    const int c0 = std::max(0,              static_cast<int>(std::floor(bb.left())));
    const int c1 = std::min((int)cols_ - 1, static_cast<int>(std::ceil(bb.right())));

    for (int r = r0; r <= r1; ++r)
        for (int c = c0; c <= c1; ++c)
            if (pointInPolygon(poly, static_cast<float>(c), static_cast<float>(r)))
                mask_[static_cast<uint32_t>(r) * cols_ + static_cast<uint32_t>(c)] = select;
}

void RoiMask::applyHorizStrip(uint32_t rowMin, uint32_t rowMax, bool select) {
    if (mask_.empty()) return;
    rowMin = std::min(rowMin, rows_ - 1);
    rowMax = std::min(rowMax, rows_ - 1);
    if (rowMin > rowMax) std::swap(rowMin, rowMax);
    for (uint32_t r = rowMin; r <= rowMax; ++r)
        for (uint32_t c = 0; c < cols_; ++c)
            mask_[r * cols_ + c] = select;
}

void RoiMask::applyVertStrip(uint32_t colMin, uint32_t colMax, bool select) {
    if (mask_.empty()) return;
    colMin = std::min(colMin, cols_ - 1);
    colMax = std::min(colMax, cols_ - 1);
    if (colMin > colMax) std::swap(colMin, colMax);
    for (uint32_t r = 0; r < rows_; ++r)
        for (uint32_t c = colMin; c <= colMax; ++c)
            mask_[r * cols_ + c] = select;
}

void RoiMask::applyEllipse(float cx, float cy, float rx, float ry, bool select) {
    if (mask_.empty() || rx <= 0.0f || ry <= 0.0f) return;
    const int r0 = std::max(0,              static_cast<int>(std::floor(cy - ry)));
    const int r1 = std::min((int)rows_ - 1, static_cast<int>(std::ceil(cy + ry)));
    const int c0 = std::max(0,              static_cast<int>(std::floor(cx - rx)));
    const int c1 = std::min((int)cols_ - 1, static_cast<int>(std::ceil(cx + rx)));
    for (int r = r0; r <= r1; ++r) {
        for (int c = c0; c <= c1; ++c) {
            const float dr = (r - cy) / ry;
            const float dc = (c - cx) / rx;
            if (dr * dr + dc * dc <= 1.0f)
                mask_[static_cast<uint32_t>(r) * cols_ + static_cast<uint32_t>(c)] = select;
        }
    }
}

// ── Z-based clipping ──────────────────────────────────────────────────────────

void RoiMask::clipToZRange(const ViffImage& img, float zMin, float zMax) {
    for (uint32_t r = 0; r < rows_; ++r)
        for (uint32_t c = 0; c < cols_; ++c) {
            float v = img.at(r, c);
            if (!img.isValid(r, c) || v < zMin || v > zMax)
                mask_[r * cols_ + c] = false;
        }
}

void RoiMask::clipToGradient(const ViffImage& img, float maxAngleDeg) {
    // Per-axis Z threshold: one pixel step in X (or Y) of physical size ps
    // corresponds to slope angle α when dZ = ps × tan(α).
    const float angleRad = maxAngleDeg * kDegToRadF;
    const float tanA     = std::tan(angleRad);
    const float thX = (img.xPixelSize > 0.0f ? img.xPixelSize : 1.0f) * tanA;
    const float thY = (img.yPixelSize > 0.0f ? img.yPixelSize : 1.0f) * tanA;

    for (uint32_t r = 0; r < rows_; ++r) {
        for (uint32_t c = 0; c < cols_; ++c) {
            if (!mask_[r * cols_ + c]) continue;
            if (!img.isValid(r, c)) { mask_[r * cols_ + c] = false; continue; }
            const float v = img.at(r, c);
            // Y-neighbours (row direction)
            if (r > 0 && img.isValid(r-1, c) && std::abs(img.at(r-1, c) - v) > thY)
                { mask_[r * cols_ + c] = false; continue; }
            if (r+1 < rows_ && img.isValid(r+1, c) && std::abs(img.at(r+1, c) - v) > thY)
                { mask_[r * cols_ + c] = false; continue; }
            // X-neighbours (col direction)
            if (c > 0 && img.isValid(r, c-1) && std::abs(img.at(r, c-1) - v) > thX)
                { mask_[r * cols_ + c] = false; continue; }
            if (c+1 < cols_ && img.isValid(r, c+1) && std::abs(img.at(r, c+1) - v) > thX)
                { mask_[r * cols_ + c] = false; continue; }
        }
    }
}

// ── Commit ────────────────────────────────────────────────────────────────────

void RoiMask::commitToImage(ViffImage& img) const {
    if (mask_.empty()) return;
    for (uint32_t r = 0; r < rows_; ++r)
        for (uint32_t c = 0; c < cols_; ++c)
            if (!mask_[r * cols_ + c])
                img.data[r * cols_ + c] = 0.0f;
}

// ── Polygon persistence ───────────────────────────────────────────────────────

bool RoiMask::savePolygon(const QString& path, const QPolygonF& poly) {
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) return false;
    QTextStream out(&f);
    for (const QPointF& pt : poly)
        out << pt.x() << ' ' << pt.y() << '\n';
    return true;
}

bool RoiMask::loadPolygon(const QString& path, QPolygonF& poly) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return false;
    QTextStream in(&f);
    poly.clear();
    double x, y;
    while (!in.atEnd()) {
        in >> x >> y;
        if (in.status() == QTextStream::Ok)
            poly << QPointF(x, y);
    }
    return poly.size() >= 3;
}
