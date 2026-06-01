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

#include <QDialog>
#include <QPointF>
#include <vector>

struct ViffImage;
class QLabel;

// A dialog that displays a 1D cross-section (slice) through a depth image.
// The slice is defined by two endpoints in image coordinates.
// X-axis: distance along the slice line (in mm)
// Y-axis: interpolated Z values (depth)
//
// Mouse interaction:
// - Moving the mouse shows coordinates in the status bar
// - Left-click sets a reference point for relative measurements
// - Right-click toggles between absolute and relative coordinate display

class SliceDialog : public QDialog {
    Q_OBJECT
public:
    SliceDialog(const ViffImage& img,
                QPointF startPt, QPointF endPt,
                QWidget* parent = nullptr);

private:
    void computeSlice();
    void onSave();

    const ViffImage& img_;
    QPointF startPt_;
    QPointF endPt_;

    // Computed slice data
    std::vector<float> sliceX_;  // Distance along slice (mm)
    std::vector<float> sliceZ_;  // Interpolated Z values
    float sliceLength_ = 0;      // Total length in mm
    float zMin_ = 0, zMax_ = 0;  // Z range for display

    // Reference point for relative measurements (in slice coordinates)
    float refX_ = 0, refZ_ = 0;
    bool  hasRef_ = false;
    bool  showRelative_ = false;

    QLabel* coordLabel_ = nullptr;

    friend class SliceView;
};
