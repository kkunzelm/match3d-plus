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

    // Compute initial clip range from valid pixel values
    clipMin_ = std::numeric_limits<float>::max();
    clipMax_ = std::numeric_limits<float>::lowest();
    for (uint32_t r = 0; r < image_.rows; ++r) {
        for (uint32_t c = 0; c < image_.cols; ++c) {
            if (image_.isValid(r, c)) {
                float v = image_.at(r, c);
                clipMin_ = std::min(clipMin_, v);
                clipMax_ = std::max(clipMax_, v);
            }
        }
    }
    if (clipMin_ >= clipMax_) { clipMin_ = 0.0f; clipMax_ = 1.0f; }
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

            if (!valid) {
                line[c] = qRgb(0, 0, 0);
            } else if (!selected) {
                // Deselected pixels: 25% brightness
                QRgb col = colorForZ(image_.at(ur, uc));
                line[c] = qRgb(qRed(col) / 4, qGreen(col) / 4, qBlue(col) / 4);
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

            if (!image_.isValid(ur, uc)) {
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

            if (roiMask_ && !roiMask_->isSelected(ur, uc))
                gray /= 4;

            line[c] = qRgb(gray, gray, gray);
        }
    }
}

QRgb DepthImageView::colorForZ(float z) const {
    const float range = clipMax_ - clipMin_;
    const float t = (range > 0.0f) ? std::clamp((z - clipMin_) / range, 0.0f, 1.0f) : 0.5f;

    switch (style_) {
    case ImageWindow::Style::FalseColor:
        return jetColor(t);
    case ImageWindow::Style::MediumGray: {
        const float v = std::clamp(0.5f + (t - 0.5f), 0.0f, 1.0f);
        const int g = static_cast<int>(v * 255.0f);
        return qRgb(g, g, g);
    }
    default: {
        const int g = static_cast<int>(t * 255.0f);
        return qRgb(g, g, g);
    }
    }
}

QRgb DepthImageView::jetColor(float t) {
    auto c01 = [](float x) { return std::clamp(x, 0.0f, 1.0f); };
    const float r = c01(1.5f - std::abs(4.0f * t - 3.0f));
    const float g = c01(1.5f - std::abs(4.0f * t - 2.0f));
    const float b = c01(1.5f - std::abs(4.0f * t - 1.0f));
    return qRgb(static_cast<int>(r * 255), static_cast<int>(g * 255), static_cast<int>(b * 255));
}

QSize DepthImageView::sizeHint() const {
    return QSize(static_cast<int>(image_.cols * zoom_),
                 static_cast<int>(image_.rows * zoom_));
}

QRectF DepthImageView::imageRect() const {
    if (image_.cols == 0 || image_.rows == 0) return {};
    return QRectF(0, 0,
                  static_cast<float>(image_.cols) * zoom_,
                  static_cast<float>(image_.rows) * zoom_);
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
    if (!qimage_.isNull())
        p.drawImage(imageRect(), qimage_);

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
    } else {
        emit pixelLeft();
        if (polyMode_ != PolygonMode::None) {
            polyHasMouse_ = false;
            update();
        }
    }
}

void DepthImageView::mousePressEvent(QMouseEvent* event) {
    if (event->button() != Qt::LeftButton) return;
    int col, row;
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
