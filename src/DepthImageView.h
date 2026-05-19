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

#include "ImageWindow.h"
#include "io/ViffReader.h"

#include <QImage>
#include <QPointF>
#include <QPolygonF>
#include <QVector>
#include <QWidget>

#include <utility>

class RoiMask;

class DepthImageView : public QWidget {
    Q_OBJECT
public:
    explicit DepthImageView(const ViffImage& img, QWidget* parent = nullptr);

    void setStyle(ImageWindow::Style style);
    void setClipRange(float zMin, float zMax);
    float clipMin() const { return clipMin_; }
    float clipMax() const { return clipMax_; }

    QSize sizeHint() const override;

    void setRoiMask(const RoiMask* mask);
    void roiChanged();  // call after external ROI modification

    // When roiOnly=true, unselected pixels are drawn black (hidden).
    // When false (default), unselected pixels are dimmed to 25% brightness.
    void setRoiOnly(bool roiOnly);

    // Enter interactive polygon selection mode.
    // select=true → select pixels inside polygon; false → unselect.
    void startPolygonMode(bool select);
    void cancelPolygonMode();

    // Landmark pick mode: user clicks to add numbered landmark points.
    void startLandmarkPickMode();
    void stopLandmarkPickMode();
    void setLandmarkDisplay(const QVector<QPointF>& pts);
    void clearLandmarkDisplay();

signals:
    void pixelHovered(int col, int row, float z);
    void pixelLeft();
    // Emitted when user double-clicks to close polygon (image coords).
    void polygonCompleted(QPolygonF imagePoly, bool select);
    // Emitted on each single click in landmark pick mode (image coords).
    void landmarkPicked(QPointF imagePos);
    // Emitted when user presses Enter or double-clicks in landmark pick mode.
    void landmarkPickingDone();

protected:
    void paintEvent(QPaintEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    void rebuildImage();
    void rebuildGrayCast();
    QRgb colorForZ(float z) const;

    bool widgetToImage(const QPointF& pos, int& col, int& row) const;
    QPointF imageToWidget(float col, float row) const;
    QRectF imageRect() const;
    std::pair<float,float> pixelScale() const;

    const ViffImage& image_;
    const RoiMask*   roiMask_ = nullptr;

    ImageWindow::Style style_ = ImageWindow::Style::Linear;
    float clipMin_;
    float clipMax_;

    QImage qimage_;
    bool   dirty_   = true;
    bool   roiOnly_ = false;

    float   zoom_  = 1.0f;

    // Polygon mode state
    enum class PolygonMode { None, Select, Unselect };
    PolygonMode  polyMode_    = PolygonMode::None;
    QPolygonF    polyVerts_;   // image coordinates (col, row)
    QPointF      polyMouse_;   // current mouse image coordinates (for rubber band)
    bool         polyHasMouse_ = false;

    // Landmark pick mode state
    bool             landmarkMode_    = false;
    QVector<QPointF> landmarkDisplay_;  // image coords of displayed landmarks
};
