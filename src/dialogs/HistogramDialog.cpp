#include "HistogramDialog.h"
#include "../DepthImageView.h"
#include "../RoiMask.h"
#include "../io/ViffReader.h"

#include <QAction>
#include <QFile>
#include <QPushButton>
#include <QFileDialog>
#include <QFont>
#include <QMenuBar>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QTextStream>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <vector>

// ── Internal histogram painting widget ───────────────────────────────────────
// Not a Q_OBJECT — uses std::function callbacks for mouse events.

class HistoView : public QWidget {
public:
    explicit HistoView(QWidget* parent = nullptr) : QWidget(parent) {
        setMinimumSize(300, 180);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    }

    void setData(const std::vector<uint32_t>& bins, float zMin, float zMax,
                 uint32_t maxCount, uint32_t totalValid) {
        bins_       = bins;
        zMin_       = zMin;
        zMax_       = zMax;
        maxCount_   = maxCount;
        totalValid_ = totalValid;
        update();
    }

    // Callbacks set by HistogramDialog
    std::function<void(float)> onMinClicked;
    std::function<void(float)> onMaxClicked;

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        const QRect area = histArea();

        p.fillRect(rect(), Qt::black);
        p.fillRect(area, QColor(20, 20, 20));
        p.setPen(QColor(80, 80, 80));
        p.drawRect(area);

        // Bars (white, per original spec)
        if (maxCount_ > 0 && !bins_.empty()) {
            const float barW = static_cast<float>(area.width()) / bins_.size();
            p.setPen(Qt::NoPen);
            p.setBrush(Qt::white);
            for (int i = 0; i < static_cast<int>(bins_.size()); ++i) {
                if (bins_[i] == 0) continue;
                const int barH = static_cast<int>(
                    static_cast<double>(bins_[i]) / maxCount_ * area.height());
                const int x = area.left() + static_cast<int>(i * barW);
                const int w = std::max(1, static_cast<int>((i + 1) * barW) -
                                           static_cast<int>(i * barW));
                p.drawRect(x, area.bottom() - barH + 1, w, barH);
            }
        }

        p.setFont(QFont("sans", 8));

        // X-axis labels: current zMin and zMax
        p.setPen(Qt::white);
        const QString minStr = QString::number(static_cast<double>(zMin_), 'g', 5);
        const QString maxStr = QString::number(static_cast<double>(zMax_), 'g', 5);
        p.drawText(area.left(), area.bottom() + 14, minStr);
        p.drawText(area.right() - p.fontMetrics().horizontalAdvance(maxStr),
                   area.bottom() + 14, maxStr);

        // Y-axis: top label = % of total for the peak bin
        if (totalValid_ > 0 && maxCount_ > 0) {
            const double maxPct = static_cast<double>(maxCount_) / totalValid_ * 100.0;
            p.drawText(2, area.top() + 10,
                       QString("%1%").arg(maxPct, 0, 'f', 1));
        }
        p.drawText(2, area.bottom(), "0%");

        // Interaction hints
        p.setPen(QColor(200, 100, 100));
        p.drawText(area.left() + 4, area.top() - 4, "L: set min");
        p.setPen(QColor(100, 100, 200));
        const QString rHint = "R: set max";
        p.drawText(area.right() - p.fontMetrics().horizontalAdvance(rHint),
                   area.top() - 4, rHint);
    }

    void mousePressEvent(QMouseEvent* event) override {
        const QRect area = histArea();
        const int   cx   = std::clamp(static_cast<int>(event->position().x()),
                                      area.left(), area.right());
        const float frac = static_cast<float>(cx - area.left()) /
                           std::max(1, area.width());
        const float z = zMin_ + frac * (zMax_ - zMin_);

        if (event->button() == Qt::LeftButton && onMinClicked)
            onMinClicked(z);
        else if (event->button() == Qt::RightButton && onMaxClicked)
            onMaxClicked(z);
    }

private:
    QRect histArea() const {
        // margins: left for % label, bottom for z labels, top for hints
        return QRect(46, 22, width() - 66, height() - 42);
    }

    std::vector<uint32_t> bins_;
    float    zMin_ = 0, zMax_ = 0;
    uint32_t maxCount_ = 0, totalValid_ = 0;
};

// ── HistogramDialog ───────────────────────────────────────────────────────────

HistogramDialog::HistogramDialog(const ViffImage& img, const RoiMask* roi,
                                  DepthImageView* depthView,
                                  std::function<void(float, float)> clipCallback,
                                  QWidget* parent)
    : QDialog(parent)
    , img_(img)
    , roi_(roi)
    , depthView_(depthView)
    , clipCallback_(std::move(clipCallback))
{
    setWindowTitle("Histogram");
    setWindowFlags(windowFlags() | Qt::WindowMaximizeButtonHint);
    resize(600, 380);

    // Determine true data range (needed to clamp clip adjustments)
    dataMin_ = std::numeric_limits<float>::max();
    dataMax_ = std::numeric_limits<float>::lowest();
    for (uint32_t r = 0; r < img_.rows; ++r)
        for (uint32_t c = 0; c < img_.cols; ++c) {
            if (!img_.isValid(r, c)) continue;
            if (roi_ && !roi_->isSelected(r, c)) continue;
            const float v = img_.at(r, c);
            dataMin_ = std::min(dataMin_, v);
            dataMax_ = std::max(dataMax_, v);
        }
    if (dataMin_ >= dataMax_) { dataMin_ = 0.0f; dataMax_ = 1.0f; }

    // Start from the view's current clip range
    zMin_ = depthView_ ? depthView_->clipMin() : dataMin_;
    zMax_ = depthView_ ? depthView_->clipMax() : dataMax_;
    // Clamp to data range so we don't start outside the actual data
    zMin_ = std::clamp(zMin_, dataMin_, dataMax_);
    zMax_ = std::clamp(zMax_, dataMin_, dataMax_);
    if (zMin_ >= zMax_) { zMin_ = dataMin_; zMax_ = dataMax_; }

    // Menu
    auto* mb       = new QMenuBar(this);
    auto* fileMenu = mb->addMenu("&File");
    connect(fileMenu->addAction("&Save..."), &QAction::triggered,
            this, &HistogramDialog::onSave);
    fileMenu->addSeparator();
    connect(fileMenu->addAction("&Close"), &QAction::triggered,
            this, &QDialog::accept);

    auto* layout = new QVBoxLayout(this);
    layout->setMenuBar(mb);
    layout->setContentsMargins(4, 4, 4, 4);

    histoView_ = new HistoView(this);
    layout->addWidget(histoView_);

    histoView_->onMinClicked = [this](float z) { applyMin(z); };
    histoView_->onMaxClicked = [this](float z) { applyMax(z); };

    if (clipCallback_) {
        auto* clipBtn = new QPushButton("Clip to Z range", this);
        clipBtn->setToolTip("Unselect all ROI pixels outside the current [min, max] range");
        layout->addWidget(clipBtn);
        connect(clipBtn, &QPushButton::clicked, this, [this]{
            if (clipCallback_) clipCallback_(zMin_, zMax_);
        });
    }

    rebuild();
}

void HistogramDialog::rebuild() {
    const float range = zMax_ - zMin_;
    bins_.assign(kBins, 0);
    maxCount_   = 0;
    totalValid_ = 0;

    if (range > 0.0f) {
        for (uint32_t r = 0; r < img_.rows; ++r) {
            for (uint32_t c = 0; c < img_.cols; ++c) {
                if (!img_.isValid(r, c)) continue;
                if (roi_ && !roi_->isSelected(r, c)) continue;
                ++totalValid_;
                const float v = img_.at(r, c);
                if (v < zMin_ || v > zMax_) continue; // out-of-range → not binned
                const int bin = std::clamp(
                    static_cast<int>((v - zMin_) / range * kBins), 0, kBins - 1);
                ++bins_[static_cast<size_t>(bin)];
            }
        }
        for (uint32_t n : bins_) maxCount_ = std::max(maxCount_, n);
    }

    histoView_->setData(bins_, zMin_, zMax_, maxCount_, totalValid_);
}

void HistogramDialog::applyMin(float z) {
    z = std::clamp(z, dataMin_, zMax_ - 1e-6f);
    if (z >= zMax_) return;
    zMin_ = z;
    if (depthView_) depthView_->setClipRange(zMin_, zMax_);
    rebuild();
}

void HistogramDialog::applyMax(float z) {
    z = std::clamp(z, zMin_ + 1e-6f, dataMax_);
    if (z <= zMin_) return;
    zMax_ = z;
    if (depthView_) depthView_->setClipRange(zMin_, zMax_);
    rebuild();
}

void HistogramDialog::onSave() {
    QString path = QFileDialog::getSaveFileName(
        this, "Save histogram", QString(), "Text files (*.txt);;All files (*)");
    if (path.isEmpty()) return;
    // Auto-append .txt if no extension present
    if (!path.contains('.'))
        path += ".txt";

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "Save histogram", "Cannot write file:\n" + path);
        return;
    }

    QTextStream out(&f);
    out << "# match3d histogram\n";
    out << "# ZMin = " << zMin_ << "\n";
    out << "# ZMax = " << zMax_ << "\n";
    out << "# TotalValidPixels = " << totalValid_ << "\n";
    out << "# Format: z_center  count  percent\n";

    const float binWidth = (zMax_ - zMin_) / kBins;
    for (int i = 0; i < kBins; ++i) {
        const float zCenter = zMin_ + (i + 0.5f) * binWidth;
        const double pct = totalValid_ > 0
            ? static_cast<double>(bins_[i]) / totalValid_ * 100.0 : 0.0;
        out << QString::number(static_cast<double>(zCenter), 'g', 7)
            << "\t" << bins_[i]
            << "\t" << QString::number(pct, 'f', 4)
            << "\n";
    }
}
