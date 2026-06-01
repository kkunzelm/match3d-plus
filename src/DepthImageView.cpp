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

#include "DepthImageView.h"
#include "RoiMask.h"

#include <QFont>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>
#include <limits>

DepthImageView::DepthImageView(const ViffImage& img, QWidget* parent)
    : QWidget(parent), image_(img)
{
    setMouseTracking(true);
    setFocusPolicy(Qt::ClickFocus);
    setMinimumSize(100, 100);

    // Compute initial clip range and statistics from valid pixel values
    clipMin_ = std::numeric_limits<float>::max();
    clipMax_ = std::numeric_limits<float>::lowest();
    double sum = 0.0;
    size_t count = 0;
    for (uint32_t r = 0; r < image_.rows; ++r) {
        for (uint32_t c = 0; c < image_.cols; ++c) {
            if (image_.isValid(r, c)) {
                float v = image_.at(r, c);
                clipMin_ = std::min(clipMin_, v);
                clipMax_ = std::max(clipMax_, v);
                sum += v;
                ++count;
            }
        }
    }
    if (clipMin_ >= clipMax_) { clipMin_ = 0.0f; clipMax_ = 1.0f; }

    // Compute mean and stddev for Linear2 style (mean ± 3*stddev)
    if (count > 0) {
        const double mean = sum / count;
        double sumSqDiff = 0.0;
        for (uint32_t r = 0; r < image_.rows; ++r) {
            for (uint32_t c = 0; c < image_.cols; ++c) {
                if (image_.isValid(r, c)) {
                    double diff = image_.at(r, c) - mean;
                    sumSqDiff += diff * diff;
                }
            }
        }
        const double stddev = std::sqrt(sumSqDiff / count);
        linear2Min_ = static_cast<float>(mean - 3.0 * stddev);
        linear2Max_ = static_cast<float>(mean + 3.0 * stddev);
    } else {
        linear2Min_ = clipMin_;
        linear2Max_ = clipMax_;
    }

    // Set initial zoom with two constraints:
    // 1. Correct physical aspect ratio: use (cols*sx) × (rows*sy) as the logical image size
    //    so that anisotropic pixels (xPixelSize ≠ yPixelSize) are rendered without distortion.
    // 2. No downsampling: zoom_ >= 1.0 ensures every data row/column maps to at least one
    //    screen pixel, so the displayed gray values match the source data exactly.
    //    (Below 1.0, Qt drops rows when scaling the QImage, producing wrong pixel values.)
    const auto [sx, sy] = pixelScale();
    const float physW = image_.cols * sx;
    const float physH = image_.rows * sy;
    const float fitZoom = (physW > 0.0f && physH > 0.0f)
        ? std::min(700.0f / physW, 550.0f / physH) : 1.0f;
    zoom_ = std::clamp(std::max(fitZoom, 1.0f), 0.05f, 32.0f);
}

// ── Public setters ────────────────────────────────────────────────────────────

void DepthImageView::setStyle(ImageWindow::Style style) {
    if (style_ == style) return;
    style_ = style;
    dirty_ = true;
    update();
}

void DepthImageView::setClipRange(float zMin, float zMax) {
    clipMin_ = zMin;
    clipMax_  = zMax;
    dirty_    = true;
    update();
}

void DepthImageView::setRoiMask(const RoiMask* mask) {
    roiMask_ = mask;
    dirty_   = true;
    update();
}

void DepthImageView::setRoiOnly(bool roiOnly) {
    if (roiOnly_ == roiOnly) return;
    roiOnly_ = roiOnly;
    dirty_   = true;
    update();
}

void DepthImageView::roiChanged() {
    dirty_ = true;
    update();
}

void DepthImageView::startPolygonMode(bool select) {
    polyMode_    = select ? PolygonMode::Select : PolygonMode::Unselect;
    polyVerts_.clear();
    polyHasMouse_ = false;
    setCursor(Qt::CrossCursor);
    update();
}

void DepthImageView::cancelPolygonMode() {
    polyMode_ = PolygonMode::None;
    polyVerts_.clear();
    unsetCursor();
    update();
}

void DepthImageView::startLandmarkPickMode() {
    cancelPolygonMode();  // mutually exclusive with polygon mode
    landmarkMode_ = true;
    setCursor(Qt::CrossCursor);
    setFocus();
}

void DepthImageView::stopLandmarkPickMode() {
    landmarkMode_ = false;
    unsetCursor();
    update();
}

void DepthImageView::setLandmarkDisplay(const QVector<QPointF>& pts) {
    landmarkDisplay_ = pts;
    update();
}

void DepthImageView::clearLandmarkDisplay() {
    landmarkDisplay_.clear();
    update();
}

void DepthImageView::startSlicePickMode() {
    cancelPolygonMode();
    stopLandmarkPickMode();
    sliceMode_ = true;
    sliceHasStart_ = false;
    sliceHasMouse_ = false;
    setCursor(Qt::CrossCursor);
    setFocus();
    update();
}

void DepthImageView::cancelSlicePickMode() {
    sliceMode_ = false;
    sliceHasStart_ = false;
    sliceHasMouse_ = false;
    unsetCursor();
    update();
}

// ── Rendering ─────────────────────────────────────────────────────────────────

void DepthImageView::rebuildImage() {
    const int w = static_cast<int>(image_.cols);
    const int h = static_cast<int>(image_.rows);
    qimage_ = QImage(w, h, QImage::Format_RGB32);

    if (style_ == ImageWindow::Style::GrayCast) {
        rebuildGrayCast();
        dirty_ = false;
        return;
    }

    for (int r = 0; r < h; ++r) {
        QRgb* line = reinterpret_cast<QRgb*>(qimage_.scanLine(r));
        for (int c = 0; c < w; ++c) {
            const uint32_t ur = static_cast<uint32_t>(r);
            const uint32_t uc = static_cast<uint32_t>(c);
            const bool valid = image_.isValid(ur, uc);
            const bool selected = !roiMask_ || roiMask_->isSelected(ur, uc);

            if (!valid || (!selected && roiOnly_)) {
                line[c] = qRgb(0, 0, 0);
            } else if (!selected) {
                // Deselected pixels: blend with red tint (preserves detail visibility)
                QRgb col = colorForZ(image_.at(ur, uc));
                // 70% original + 30% red overlay
                const int r = std::min(255, qRed(col)   * 7 / 10 + 77);
                const int g = qGreen(col) * 7 / 10;
                const int b = qBlue(col)  * 7 / 10;
                line[c] = qRgb(r, g, b);
            } else {
                line[c] = colorForZ(image_.at(ur, uc));
            }
        }
    }
    dirty_ = false;
}

void DepthImageView::rebuildGrayCast() {
    // Sobel kernels — same as in the original GreyAnimateKH_ ImageJ plugin.
    // kernelX computes the horizontal gradient, kernelY the vertical gradient.
    // gx = sobel_x / (8 * dx),  gy = sobel_y / (8 * dy)
    // The factor 8 = 4 (kernel normalisation) × 2 (centred finite difference).
    // theta   = atan( sqrt(gx² + gy²) )
    // shading = cos(theta)  — Lambertian diffuse reflection with zenith light
    // Flat surfaces (cos == 1) are rendered black, matching the scanner convention
    // that zero-gradient areas and invalid pixels both appear as "no signal".
    static constexpr int kx[3][3] = {{-1, 0, 1}, {-2, 0, 2}, {-1, 0, 1}};
    static constexpr int ky[3][3] = {{ 1, 2, 1}, { 0, 0, 0}, {-1,-2,-1}};

    const int w  = static_cast<int>(image_.cols);
    const int h  = static_cast<int>(image_.rows);
    // Normalize gradient by the clip Z-range relative to image size so that the
    // shading is independent of physical units (µm, mm, m all give the same result).
    // A slope that spans the full clip range over the full image maps to theta = 45°.
    const float  zRange    = clipMax_ - clipMin_;
    const double refPxSize = (zRange > 0.0f)
        ? (zRange / std::max(w, h))
        : 1.0;

    for (int r = 0; r < h; ++r) {
        QRgb* line = reinterpret_cast<QRgb*>(qimage_.scanLine(r));
        for (int c = 0; c < w; ++c) {
            const uint32_t ur = static_cast<uint32_t>(r);
            const uint32_t uc = static_cast<uint32_t>(c);

            const bool valid = image_.isValid(ur, uc);
            const bool selected = !roiMask_ || roiMask_->isSelected(ur, uc);

            if (!valid || (!selected && roiOnly_)) {
                line[c] = qRgb(0, 0, 0);
                continue;
            }

            // 3×3 Sobel convolution; clamp neighbours at image borders.
            // Raw pixel values (including zero/invalid neighbours) are used,
            // matching the original ImageJ behaviour.
            double gx = 0.0, gy = 0.0;
            for (int dr = -1; dr <= 1; ++dr) {
                for (int dc = -1; dc <= 1; ++dc) {
                    const double v = image_.at(
                        static_cast<uint32_t>(std::clamp(r + dr, 0, h - 1)),
                        static_cast<uint32_t>(std::clamp(c + dc, 0, w - 1)));
                    gx += kx[dr + 1][dc + 1] * v;
                    gy += ky[dr + 1][dc + 1] * v;
                }
            }
            gx /= (8.0 * refPxSize);
            gy /= (8.0 * refPxSize);

            const double theta = std::atan(std::sqrt(gx * gx + gy * gy));
            const float  cos_v = static_cast<float>(std::cos(theta));
            int gray = (cos_v < 1.0f) ? static_cast<int>(cos_v * 255.0f) : 0;

            if (!selected) {
                // Deselected pixels: blend with red tint (preserves detail visibility)
                // 70% original gray + 30% red overlay
                const int rr = std::min(255, gray * 7 / 10 + 77);
                const int gg = gray * 7 / 10;
                const int bb = gray * 7 / 10;
                line[c] = qRgb(rr, gg, bb);
            } else {
                line[c] = qRgb(gray, gray, gray);
            }
        }
    }
}

QRgb DepthImageView::colorForZ(float z) const {
    const float range = clipMax_ - clipMin_;
    const float t = (range > 0.0f) ? std::clamp((z - clipMin_) / range, 0.0f, 1.0f) : 0.5f;

    switch (style_) {
    case ImageWindow::Style::FalseColor: {
        // Each half-range is normalized independently (matches QlfUpdateLUT_ ImageJ plugin):
        //   negative [clipMin_, 0] → red:  brightest red at clipMin_, black at 0
        //   positive [0, clipMax_] → gray: black at 0, white at clipMax_
        // This ensures both signs are always fully visible regardless of asymmetry.
        // Values are clamped to the clip range for color mapping.
        const float zc = std::clamp(z, clipMin_, clipMax_);
        if (zc < 0.0f && clipMin_ < 0.0f)
            return qRgb(static_cast<int>(zc / clipMin_ * 255.0f), 0, 0);
        if (zc > 0.0f && clipMax_ > 0.0f) {
            const int v = static_cast<int>(zc / clipMax_ * 255.0f);
            return qRgb(v, v, v);
        }
        return qRgb(0, 0, 0);  // z==0 or degenerate clip range → black
    }
    case ImageWindow::Style::MediumGray: {
        const float v = std::clamp(0.5f + (t - 0.5f), 0.0f, 1.0f);
        const int g = static_cast<int>(v * 255.0f);
        return qRgb(g, g, g);
    }
    case ImageWindow::Style::Linear2: {
        // Scale using mean ± 3*stddev range
        const float range2 = linear2Max_ - linear2Min_;
        const float t2 = (range2 > 0.0f)
            ? std::clamp((z - linear2Min_) / range2, 0.0f, 1.0f)
            : 0.5f;
        const int g = static_cast<int>(t2 * 255.0f);
        return qRgb(g, g, g);
    }
    default: {
        const int g = static_cast<int>(t * 255.0f);
        return qRgb(g, g, g);
    }
    }
}


// Returns the per-axis scale factors (sx, sy) so that the rendered image has
// the correct physical aspect ratio. Both factors equal 1 when pixels are square.
std::pair<float,float> DepthImageView::pixelScale() const {
    const float minPs = std::min(image_.xPixelSize, image_.yPixelSize);
    if (minPs <= 0.0f) return {1.0f, 1.0f};
    return {image_.xPixelSize / minPs, image_.yPixelSize / minPs};
}

QSize DepthImageView::sizeHint() const {
    const auto [sx, sy] = pixelScale();
    return QSize(static_cast<int>(image_.cols * sx * zoom_),
                 static_cast<int>(image_.rows * sy * zoom_));
}

QRectF DepthImageView::imageRect() const {
    if (image_.cols == 0 || image_.rows == 0) return {};
    const auto [sx, sy] = pixelScale();
    return QRectF(0, 0,
                  image_.cols * sx * zoom_,
                  image_.rows * sy * zoom_);
}

QPointF DepthImageView::imageToWidget(float col, float row) const {
    const QRectF r = imageRect();
    if (r.width() <= 0 || r.height() <= 0) return {};
    return {r.left() + col / image_.cols * r.width(),
            r.top()  + row / image_.rows * r.height()};
}

// ── Event handlers ────────────────────────────────────────────────────────────

void DepthImageView::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.fillRect(rect(), Qt::black);

    if (dirty_) rebuildImage();
    if (!qimage_.isNull()) {
        // Draw through a scale transform so Qt uses nearest-neighbour sampling
        // (no row/column blending at any zoom level).
        const auto [sx, sy] = pixelScale();
        p.setRenderHint(QPainter::SmoothPixmapTransform, false);
        p.setTransform(QTransform::fromScale(sx * zoom_, sy * zoom_));
        p.drawImage(QPointF(0, 0), qimage_);
        p.resetTransform();
    }

    // Polygon overlay
    if (polyMode_ != PolygonMode::None && !polyVerts_.isEmpty()) {
        // Convert polygon vertices to widget coordinates
        QPolygonF widgetPoly;
        for (const QPointF& pt : polyVerts_)
            widgetPoly << imageToWidget(static_cast<float>(pt.x()),
                                        static_cast<float>(pt.y()));

        const QColor polyColor = (polyMode_ == PolygonMode::Select) ? Qt::yellow : Qt::red;

        QPen pen(polyColor, 1.5, Qt::SolidLine);
        p.setPen(pen);
        p.setBrush(Qt::NoBrush);
        p.drawPolyline(widgetPoly);

        // Rubber-band from last vertex to current mouse
        if (polyHasMouse_) {
            pen.setStyle(Qt::DashLine);
            p.setPen(pen);
            QPointF mouseW = imageToWidget(static_cast<float>(polyMouse_.x()),
                                           static_cast<float>(polyMouse_.y()));
            p.drawLine(widgetPoly.last(), mouseW);

            // Preview closing line
            if (polyVerts_.size() >= 2) {
                pen.setStyle(Qt::DotLine);
                p.setPen(pen);
                p.drawLine(mouseW, widgetPoly.first());
            }
        }

        // Vertex dots
        p.setPen(QPen(polyColor, 1));
        p.setBrush(polyColor);
        for (const QPointF& pt : widgetPoly)
            p.drawEllipse(pt, 3.0, 3.0);
    }

    // Landmark overlay: numbered yellow circles
    if (!landmarkDisplay_.isEmpty()) {
        p.setFont(QFont("sans", 8, QFont::Bold));
        for (int i = 0; i < landmarkDisplay_.size(); ++i) {
            const QPointF wpt = imageToWidget(
                static_cast<float>(landmarkDisplay_[i].x()),
                static_cast<float>(landmarkDisplay_[i].y()));
            p.setPen(QPen(Qt::yellow, 1.5));
            p.setBrush(Qt::NoBrush);
            p.drawEllipse(wpt, 7.0, 7.0);
            p.setPen(Qt::yellow);
            p.drawText(QRectF(wpt.x() - 7, wpt.y() - 7, 14, 14),
                       Qt::AlignCenter, QString::number(i + 1));
        }
    }

    // Slice line overlay
    if (sliceMode_ && sliceHasStart_) {
        const QPointF startW = imageToWidget(
            static_cast<float>(sliceStart_.x()),
            static_cast<float>(sliceStart_.y()));

        // Draw start point marker
        p.setPen(QPen(Qt::cyan, 2));
        p.setBrush(Qt::NoBrush);
        p.drawEllipse(startW, 5.0, 5.0);

        // Draw rubber-band line to mouse
        if (sliceHasMouse_) {
            const QPointF mouseW = imageToWidget(
                static_cast<float>(sliceMouse_.x()),
                static_cast<float>(sliceMouse_.y()));
            p.setPen(QPen(Qt::cyan, 1.5, Qt::DashLine));
            p.drawLine(startW, mouseW);
            p.drawEllipse(mouseW, 3.0, 3.0);
        }
    }
}

void DepthImageView::mouseMoveEvent(QMouseEvent* event) {
    int col, row;
    if (widgetToImage(event->position(), col, row)) {
        const float z = image_.isValid(static_cast<uint32_t>(row),
                                        static_cast<uint32_t>(col))
                        ? image_.at(static_cast<uint32_t>(row),
                                    static_cast<uint32_t>(col))
                        : std::numeric_limits<float>::quiet_NaN();
        emit pixelHovered(col, row, z);

        if (polyMode_ != PolygonMode::None) {
            polyMouse_    = {static_cast<double>(col), static_cast<double>(row)};
            polyHasMouse_ = true;
            update();
        }
        if (sliceMode_) {
            sliceMouse_ = {static_cast<double>(col), static_cast<double>(row)};
            sliceHasMouse_ = true;
            update();
        }
    } else {
        emit pixelLeft();
        if (polyMode_ != PolygonMode::None) {
            polyHasMouse_ = false;
            update();
        }
        if (sliceMode_) {
            sliceHasMouse_ = false;
            update();
        }
    }
}

void DepthImageView::mousePressEvent(QMouseEvent* event) {
    int col, row;
    if (event->button() == Qt::RightButton) {
        if (sliceMode_ && sliceHasStart_) {
            // Right-click sets the end point and completes the slice
            if (widgetToImage(event->position(), col, row)) {
                QPointF endPt(col, row);
                QPointF startPt = sliceStart_;
                cancelSlicePickMode();
                emit sliceCompleted(startPt, endPt);
            }
            return;
        }
        if (polyMode_ != PolygonMode::None) {
            // Add the right-click position itself as the closing vertex
            if (widgetToImage(event->position(), col, row))
                polyVerts_ << QPointF(col, row);
            if (polyVerts_.size() >= 3) {
                const bool selectOp = (polyMode_ == PolygonMode::Select);
                QPolygonF poly = polyVerts_;
                cancelPolygonMode();
                emit polygonCompleted(poly, selectOp);
            } else {
                cancelPolygonMode();  // not enough vertices — just cancel
            }
        }
        return;
    }
    if (event->button() != Qt::LeftButton) return;
    if (sliceMode_) {
        // Left-click sets the start point
        if (widgetToImage(event->position(), col, row)) {
            sliceStart_ = QPointF(col, row);
            sliceHasStart_ = true;
            update();
        }
        return;
    }
    if (landmarkMode_) {
        if (widgetToImage(event->position(), col, row))
            emit landmarkPicked(QPointF(col, row));
        return;
    }
    if (polyMode_ == PolygonMode::None) return;
    if (widgetToImage(event->position(), col, row))
        polyVerts_ << QPointF(col, row);
    update();
}

void DepthImageView::mouseDoubleClickEvent(QMouseEvent* event) {
    if (event->button() != Qt::LeftButton) return;
    if (landmarkMode_) {
        emit landmarkPickingDone();
        return;
    }
    if (polyMode_ == PolygonMode::None) return;
    if (polyVerts_.size() >= 3) {
        const bool selectOp = (polyMode_ == PolygonMode::Select);
        QPolygonF poly = polyVerts_;
        cancelPolygonMode();
        emit polygonCompleted(poly, selectOp);
    } else {
        cancelPolygonMode();
    }
}

void DepthImageView::keyPressEvent(QKeyEvent* event) {
    if (sliceMode_) {
        if (event->key() == Qt::Key_Escape) {
            cancelSlicePickMode();
            return;
        }
    }
    if (landmarkMode_) {
        if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
            emit landmarkPickingDone();
            return;
        }
        if (event->key() == Qt::Key_Escape) {
            stopLandmarkPickMode();
            clearLandmarkDisplay();
            return;
        }
    }
    if (event->key() == Qt::Key_Escape && polyMode_ != PolygonMode::None) {
        cancelPolygonMode();
        return;
    }
    if ((event->key() == Qt::Key_Backspace || event->key() == Qt::Key_Delete)
            && polyMode_ != PolygonMode::None) {
        if (!polyVerts_.isEmpty()) {
            polyVerts_.removeLast();
            update();
        }
        return;
    }
    if (event->key() == Qt::Key_Return && polyMode_ != PolygonMode::None) {
        if (polyVerts_.size() >= 3) {
            const bool selectOp = (polyMode_ == PolygonMode::Select);
            QPolygonF poly = polyVerts_;
            cancelPolygonMode();
            emit polygonCompleted(poly, selectOp);
        }
        return;
    }
    QWidget::keyPressEvent(event);
}

void DepthImageView::leaveEvent(QEvent*) {
    emit pixelLeft();
    if (polyMode_ != PolygonMode::None) {
        polyHasMouse_ = false;
        update();
    }
}

void DepthImageView::wheelEvent(QWheelEvent* event) {
    const float factor = (event->angleDelta().y() > 0) ? 1.15f : (1.0f / 1.15f);
    zoom_ = std::clamp(zoom_ * factor, 0.05f, 32.0f);
    updateGeometry();
    update();
}

void DepthImageView::resizeEvent(QResizeEvent*) {
    update();
}

bool DepthImageView::widgetToImage(const QPointF& pos, int& col, int& row) const {
    const QRectF r = imageRect();
    if (r.width() <= 0 || r.height() <= 0) return false;
    col = static_cast<int>((pos.x() - r.left()) / r.width()  * image_.cols);
    row = static_cast<int>((pos.y() - r.top())  / r.height() * image_.rows);
    return col >= 0 && col < static_cast<int>(image_.cols)
        && row >= 0 && row < static_cast<int>(image_.rows);
}
