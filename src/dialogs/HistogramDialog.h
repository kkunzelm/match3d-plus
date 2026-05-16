#pragma once

#include <QDialog>
#include <vector>

class ViffImage;
class RoiMask;

class HistogramDialog : public QDialog {
    Q_OBJECT
public:
    HistogramDialog(const ViffImage& img, const RoiMask* roi,
                    float zMin, float zMax, QWidget* parent = nullptr);

private:
    void buildHistogram(const ViffImage& img, const RoiMask* roi,
                        float zMin, float zMax);
    void paintHistogram(QPainter& p, const QRect& area);

    void paintEvent(QPaintEvent*) override;

    static constexpr int kBins = 256;
    std::vector<uint32_t> bins_;
    float zMin_ = 0;
    float zMax_ = 0;
    uint32_t maxCount_ = 0;
};
