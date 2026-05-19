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

#pragma once

#include <QPolygonF>
#include <QString>
#include <cstdint>
#include <vector>

class ViffImage;

// Pixel-level selection mask.  true = selected (in ROI), false = excluded.
// Layout mirrors ViffImage: mask_[row * cols_ + col].
class RoiMask {
public:
    RoiMask() = default;
    RoiMask(uint32_t rows, uint32_t cols, bool initiallySelected = true);

    bool isSelected(uint32_t row, uint32_t col) const;
    void set(uint32_t row, uint32_t col, bool sel);

    uint32_t rows() const { return rows_; }
    uint32_t cols() const { return cols_; }
    bool isEmpty() const { return mask_.empty(); }

    // Bulk operations
    void selectAll();
    void unselectAll();
    void invert();

    // Shape-based selection (image coords: col=x, row=y)
    void applyPolygon  (const QPolygonF& poly,
                        bool select);
    void applyHorizStrip(uint32_t rowMin, uint32_t rowMax, bool select);
    void applyVertStrip (uint32_t colMin, uint32_t colMax, bool select);
    void applyEllipse  (float cx, float cy, float rx, float ry, bool select);

    // Z-based clipping (marks pixels outside range as unselected)
    void clipToZRange  (const ViffImage& img, float zMin, float zMax);
    void clipToGradient(const ViffImage& img, float maxAngleDeg);

    // Permanently zero-out unselected pixels in img (data[i] = 0 if !mask_[i])
    void commitToImage(ViffImage& img) const;

    // Polygon persistence (image-coordinate vertices, one "col row" per line)
    static bool savePolygon(const QString& path, const QPolygonF& poly);
    static bool loadPolygon(const QString& path, QPolygonF& poly);

private:
    uint32_t rows_ = 0;
    uint32_t cols_ = 0;
    std::vector<bool> mask_;

    static bool pointInPolygon(const QPolygonF& poly, float col, float row);
};
