#include "HistogramDialog.h"
#include "../RoiMask.h"
#include "../io/ViffReader.h"

#include <QDialogButtonBox>
#include <QPainter>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

HistogramDialog::HistogramDialog(const ViffImage& img, const RoiMask* roi,
                                  float zMin, float zMax, QWidget* parent)
    : QDialog(parent), zMin_(zMin), zMax_(zMax)
{
    setWindowTitle("Histogram");
    resize(500, 320);

    auto* layout = new QVBoxLayout(this);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::accept);
    layout->addStretch();
    layout->addWidget(buttons);

    buildHistogram(img, roi, zMin, zMax);
}

void HistogramDialog::buildHistogram(const ViffImage& img, const RoiMask* roi,
                                      float zMin, float zMax) {
    bins_.assign(kBins, 0);
    maxCount_ = 0;

    const float range = zMax - zMin;
    if (range <= 0.0f) return;

    for (uint32_t r = 0; r < img.rows; ++r) {
        for (uint32_t c = 0; c < img.cols; ++c) {
            if (!img.isValid(r, c)) continue;
            if (roi && !roi->isSelected(r, c)) continue;
            const float v = img.at(r, c);
            const int bin = std::clamp(static_cast<int>((v - zMin) / range * kBins),
                                       0, kBins - 1);
            ++bins_[static_cast<size_t>(bin)];
        }
    }
    for (uint32_t n : bins_) maxCount_ = std::max(maxCount_, n);
}

void HistogramDialog::paintEvent(QPaintEvent*) {
    QPainter p(this);
    const int margin = 40;
    const QRect area(margin, margin,
                     width() - 2 * margin,
                     height() - margin - 60);  // leave room for buttons
    paintHistogram(p, area);
}

void HistogramDialog::paintHistogram(QPainter& p, const QRect& area) {
    p.fillRect(area, QColor(20, 20, 20));
    p.setPen(QColor(120, 120, 120));
    p.drawRect(area);

    if (maxCount_ == 0) return;

    const float barW = static_cast<float>(area.width()) / kBins;

    p.setPen(Qt::NoPen);
    p.setBrush(QColor(80, 180, 255));

    for (int i = 0; i < kBins; ++i) {
        const int barH = static_cast<int>(
            static_cast<float>(bins_[static_cast<size_t>(i)]) /
            static_cast<float>(maxCount_) * area.height());
        const int x = area.left() + static_cast<int>(i * barW);
        const int w = std::max(1, static_cast<int>((i + 1) * barW) - static_cast<int>(i * barW));
        p.drawRect(x, area.bottom() - barH, w, barH);
    }

    // X-axis labels (min and max)
    p.setPen(Qt::white);
    p.setFont(QFont("sans", 8));
    p.drawText(area.left(),  area.bottom() + 15, QString::number(static_cast<double>(zMin_), 'f', 0));
    const QString maxStr = QString::number(static_cast<double>(zMax_), 'f', 0);
    p.drawText(area.right() - p.fontMetrics().horizontalAdvance(maxStr),
               area.bottom() + 15, maxStr);

    // Peak count label
    p.drawText(area.left() + 4, area.top() + 14,
               QString("max: %1").arg(maxCount_));
}
