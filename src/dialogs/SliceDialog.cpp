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

#include "SliceDialog.h"
#include "../io/ViffReader.h"

#include <QAction>
#include <QFile>
#include <QFileDialog>
#include <QFont>
#include <QLabel>
#include <QMenuBar>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QTextStream>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <limits>

// ── Internal slice painting widget ────────────────────────────────────────────

class SliceView : public QWidget {
public:
    explicit SliceView(SliceDialog* dlg, QWidget* parent = nullptr)
        : QWidget(parent), dlg_(dlg)
    {
        setMinimumSize(400, 250);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        setMouseTracking(true);
    }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        const QRect area = plotArea();

        p.fillRect(rect(), Qt::black);
        p.fillRect(area, QColor(20, 20, 20));
        p.setPen(QColor(80, 80, 80));
        p.drawRect(area);

        // Draw grid lines
        p.setPen(QColor(50, 50, 50));
        for (int i = 1; i < 4; ++i) {
            int y = area.top() + i * area.height() / 4;
            p.drawLine(area.left(), y, area.right(), y);
        }
        for (int i = 1; i < 4; ++i) {
            int x = area.left() + i * area.width() / 4;
            p.drawLine(x, area.top(), x, area.bottom());
        }

        // Draw slice profile
        if (!dlg_->sliceX_.empty() && dlg_->sliceLength_ > 0 &&
            dlg_->zMax_ > dlg_->zMin_) {
            p.setPen(QPen(Qt::white, 1.5));
            const float xScale = static_cast<float>(area.width()) / dlg_->sliceLength_;
            const float zRange = dlg_->zMax_ - dlg_->zMin_;
            const float zScale = static_cast<float>(area.height()) / zRange;

            QPointF prevPt;
            bool havePrev = false;
            for (size_t i = 0; i < dlg_->sliceX_.size(); ++i) {
                if (std::isnan(dlg_->sliceZ_[i])) {
                    havePrev = false;
                    continue;
                }
                const float px = area.left() + dlg_->sliceX_[i] * xScale;
                const float py = area.bottom() - (dlg_->sliceZ_[i] - dlg_->zMin_) * zScale;
                QPointF pt(px, py);
                if (havePrev) {
                    p.drawLine(prevPt, pt);
                }
                prevPt = pt;
                havePrev = true;
            }
        }

        // Draw reference point marker if set
        if (dlg_->hasRef_ && dlg_->sliceLength_ > 0 && dlg_->zMax_ > dlg_->zMin_) {
            const float xScale = static_cast<float>(area.width()) / dlg_->sliceLength_;
            const float zRange = dlg_->zMax_ - dlg_->zMin_;
            const float zScale = static_cast<float>(area.height()) / zRange;
            const float px = area.left() + dlg_->refX_ * xScale;
            const float py = area.bottom() - (dlg_->refZ_ - dlg_->zMin_) * zScale;
            p.setPen(QPen(Qt::yellow, 2));
            p.drawLine(QPointF(px - 5, py), QPointF(px + 5, py));
            p.drawLine(QPointF(px, py - 5), QPointF(px, py + 5));
        }

        // Axis labels
        p.setFont(QFont("sans", 8));
        p.setPen(Qt::white);

        // X-axis: 0 and sliceLength
        p.drawText(area.left(), area.bottom() + 14, "0");
        const QString maxXStr = QString::number(static_cast<double>(dlg_->sliceLength_), 'f', 3) + " mm";
        p.drawText(area.right() - p.fontMetrics().horizontalAdvance(maxXStr),
                   area.bottom() + 14, maxXStr);

        // Y-axis: zMin and zMax
        const QString minZStr = QString::number(static_cast<double>(dlg_->zMin_), 'f', 2);
        const QString maxZStr = QString::number(static_cast<double>(dlg_->zMax_), 'f', 2);
        p.drawText(2, area.bottom(), minZStr);
        p.drawText(2, area.top() + 12, maxZStr);

        // Mode indicator
        p.setPen(dlg_->showRelative_ ? Qt::cyan : QColor(150, 150, 150));
        const QString modeStr = dlg_->showRelative_ ? "Relative (dx, dy)" : "Absolute (x, y)";
        p.drawText(area.left() + 4, area.top() - 4, modeStr);

        // Hint
        p.setPen(QColor(100, 100, 100));
        p.drawText(area.right() - 180, area.top() - 4, "L: set ref  R: toggle mode");
    }

    void mouseMoveEvent(QMouseEvent* event) override {
        const QRect area = plotArea();
        if (!area.contains(event->pos()) ||
            dlg_->sliceLength_ <= 0 || dlg_->sliceX_.empty()) {
            dlg_->coordLabel_->setText("---");
            return;
        }

        // Convert mouse position to slice coordinates
        const float frac = static_cast<float>(event->position().x() - area.left()) /
                           std::max(1, area.width());
        const float x = frac * dlg_->sliceLength_;

        // Find nearest slice sample and interpolate Z
        float z = std::numeric_limits<float>::quiet_NaN();
        for (size_t i = 0; i + 1 < dlg_->sliceX_.size(); ++i) {
            if (x >= dlg_->sliceX_[i] && x <= dlg_->sliceX_[i + 1]) {
                const float t = (x - dlg_->sliceX_[i]) /
                                (dlg_->sliceX_[i + 1] - dlg_->sliceX_[i]);
                if (!std::isnan(dlg_->sliceZ_[i]) && !std::isnan(dlg_->sliceZ_[i + 1])) {
                    z = dlg_->sliceZ_[i] * (1 - t) + dlg_->sliceZ_[i + 1] * t;
                }
                break;
            }
        }

        QString text;
        if (dlg_->showRelative_ && dlg_->hasRef_) {
            const float dx = x - dlg_->refX_;
            const float dy = std::isnan(z) ? 0 : z - dlg_->refZ_;
            text = QString("dx=%1 mm  dy=%2")
                       .arg(static_cast<double>(dx), 0, 'f', 4)
                       .arg(std::isnan(z) ? QString("---") :
                            QString::number(static_cast<double>(dy), 'f', 4));
        } else {
            text = QString("x=%1 mm  y=%2")
                       .arg(static_cast<double>(x), 0, 'f', 4)
                       .arg(std::isnan(z) ? QString("---") :
                            QString::number(static_cast<double>(z), 'f', 4));
        }
        dlg_->coordLabel_->setText(text);
    }

    void mousePressEvent(QMouseEvent* event) override {
        const QRect area = plotArea();
        if (!area.contains(event->pos()) || dlg_->sliceLength_ <= 0)
            return;

        if (event->button() == Qt::LeftButton) {
            // Set reference point
            const float frac = static_cast<float>(event->position().x() - area.left()) /
                               std::max(1, area.width());
            const float x = frac * dlg_->sliceLength_;

            // Find Z at this position
            float z = 0;
            for (size_t i = 0; i + 1 < dlg_->sliceX_.size(); ++i) {
                if (x >= dlg_->sliceX_[i] && x <= dlg_->sliceX_[i + 1]) {
                    const float t = (x - dlg_->sliceX_[i]) /
                                    (dlg_->sliceX_[i + 1] - dlg_->sliceX_[i]);
                    if (!std::isnan(dlg_->sliceZ_[i]) && !std::isnan(dlg_->sliceZ_[i + 1])) {
                        z = dlg_->sliceZ_[i] * (1 - t) + dlg_->sliceZ_[i + 1] * t;
                    }
                    break;
                }
            }
            dlg_->refX_ = x;
            dlg_->refZ_ = z;
            dlg_->hasRef_ = true;
            update();
        } else if (event->button() == Qt::RightButton) {
            // Toggle absolute/relative mode
            dlg_->showRelative_ = !dlg_->showRelative_;
            update();
        }
    }

    void leaveEvent(QEvent*) override {
        dlg_->coordLabel_->setText("---");
    }

private:
    QRect plotArea() const {
        // margins: left for Z labels, bottom for X labels, top for hints
        return QRect(50, 20, width() - 70, height() - 45);
    }

    SliceDialog* dlg_;
};

// ── SliceDialog ───────────────────────────────────────────────────────────────

SliceDialog::SliceDialog(const ViffImage& img,
                         QPointF startPt, QPointF endPt,
                         QWidget* parent)
    : QDialog(parent)
    , img_(img)
    , startPt_(startPt)
    , endPt_(endPt)
{
    setWindowTitle("Slice");
    setWindowFlags(windowFlags() | Qt::WindowMaximizeButtonHint);
    resize(600, 400);

    // Menu
    auto* mb       = new QMenuBar(this);
    auto* fileMenu = mb->addMenu("&File");
    connect(fileMenu->addAction("&Save..."), &QAction::triggered,
            this, &SliceDialog::onSave);
    fileMenu->addSeparator();
    connect(fileMenu->addAction("&Close"), &QAction::triggered,
            this, &QDialog::accept);

    auto* layout = new QVBoxLayout(this);
    layout->setMenuBar(mb);
    layout->setContentsMargins(4, 4, 4, 4);

    auto* sliceView = new SliceView(this, this);
    layout->addWidget(sliceView);

    coordLabel_ = new QLabel("---", this);
    coordLabel_->setStyleSheet("QLabel { font-family: monospace; padding: 4px; }");
    layout->addWidget(coordLabel_);

    computeSlice();
}

void SliceDialog::computeSlice() {
    // Vector from start to end in image coordinates (col, row)
    const float dx = static_cast<float>(endPt_.x() - startPt_.x());
    const float dy = static_cast<float>(endPt_.y() - startPt_.y());

    // Physical length of the slice (using pixel sizes)
    const float physDx = dx * img_.xPixelSize;
    const float physDy = dy * img_.yPixelSize;
    sliceLength_ = std::sqrt(physDx * physDx + physDy * physDy);

    if (sliceLength_ <= 0) return;

    // Number of samples: at least as many as the longer dimension traversed
    const int numSamples = std::max(2, static_cast<int>(
        std::sqrt(dx * dx + dy * dy) * 2));

    sliceX_.resize(numSamples);
    sliceZ_.resize(numSamples);

    zMin_ = std::numeric_limits<float>::max();
    zMax_ = std::numeric_limits<float>::lowest();

    for (int i = 0; i < numSamples; ++i) {
        const float t = static_cast<float>(i) / (numSamples - 1);

        // Position along slice in physical units (mm)
        sliceX_[i] = t * sliceLength_;

        // Position in image coordinates (floating point)
        const float col = static_cast<float>(startPt_.x()) + t * dx;
        const float row = static_cast<float>(startPt_.y()) + t * dy;

        // Bilinear interpolation
        const int c0 = static_cast<int>(std::floor(col));
        const int r0 = static_cast<int>(std::floor(row));
        const int c1 = c0 + 1;
        const int r1 = r0 + 1;

        // Check bounds
        if (c0 < 0 || r0 < 0 ||
            c1 >= static_cast<int>(img_.cols) ||
            r1 >= static_cast<int>(img_.rows)) {
            sliceZ_[i] = std::numeric_limits<float>::quiet_NaN();
            continue;
        }

        // Get four corner values
        const float z00 = img_.at(static_cast<uint32_t>(r0), static_cast<uint32_t>(c0));
        const float z01 = img_.at(static_cast<uint32_t>(r0), static_cast<uint32_t>(c1));
        const float z10 = img_.at(static_cast<uint32_t>(r1), static_cast<uint32_t>(c0));
        const float z11 = img_.at(static_cast<uint32_t>(r1), static_cast<uint32_t>(c1));

        // Check validity (using simple check - 0 is invalid for depth images)
        const bool v00 = img_.isValid(static_cast<uint32_t>(r0), static_cast<uint32_t>(c0));
        const bool v01 = img_.isValid(static_cast<uint32_t>(r0), static_cast<uint32_t>(c1));
        const bool v10 = img_.isValid(static_cast<uint32_t>(r1), static_cast<uint32_t>(c0));
        const bool v11 = img_.isValid(static_cast<uint32_t>(r1), static_cast<uint32_t>(c1));

        if (!v00 || !v01 || !v10 || !v11) {
            sliceZ_[i] = std::numeric_limits<float>::quiet_NaN();
            continue;
        }

        // Bilinear interpolation
        const float fc = col - c0;
        const float fr = row - r0;
        const float z0 = z00 * (1 - fc) + z01 * fc;
        const float z1 = z10 * (1 - fc) + z11 * fc;
        sliceZ_[i] = z0 * (1 - fr) + z1 * fr;

        zMin_ = std::min(zMin_, sliceZ_[i]);
        zMax_ = std::max(zMax_, sliceZ_[i]);
    }

    // Add some margin to Z range
    if (zMin_ < zMax_) {
        const float margin = (zMax_ - zMin_) * 0.05f;
        zMin_ -= margin;
        zMax_ += margin;
    } else {
        zMin_ = 0;
        zMax_ = 1;
    }
}

void SliceDialog::onSave() {
    QString path = QFileDialog::getSaveFileName(
        this, "Save slice data", QString(), "Text files (*.txt);;All files (*)");
    if (path.isEmpty()) return;
    if (!path.contains('.'))
        path += ".txt";

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "Save slice", "Cannot write file:\n" + path);
        return;
    }

    QTextStream out(&f);
    out << "# Match3D+ slice data\n";
    out << "# Start point (col, row): " << startPt_.x() << ", " << startPt_.y() << "\n";
    out << "# End point (col, row): " << endPt_.x() << ", " << endPt_.y() << "\n";
    out << "# Slice length: " << sliceLength_ << " mm\n";
    out << "# Format: distance_mm  z_value\n";

    for (size_t i = 0; i < sliceX_.size(); ++i) {
        if (std::isnan(sliceZ_[i])) continue;
        out << QString::number(static_cast<double>(sliceX_[i]), 'f', 6)
            << "\t" << QString::number(static_cast<double>(sliceZ_[i]), 'f', 6)
            << "\n";
    }
}
