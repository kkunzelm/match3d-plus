#pragma once

#include <QDialog>
#include <functional>
#include <vector>

class ViffImage;
class RoiMask;
class DepthImageView;
class HistoView;

class HistogramDialog : public QDialog {
    Q_OBJECT
public:
    // depthView is updated live as the user adjusts the clip range.
    // clipCallback is called when the user clicks "Clip to Z range";
    // it receives the current (zMin, zMax) and should update the ROI mask.
    HistogramDialog(const ViffImage& img, const RoiMask* roi,
                    DepthImageView* depthView,
                    std::function<void(float, float)> clipCallback,
                    QWidget* parent = nullptr);

private:
    void rebuild();
    void applyMin(float z);
    void applyMax(float z);
    void onSave();

    const ViffImage& img_;
    const RoiMask*   roi_;
    DepthImageView*  depthView_;
    std::function<void(float, float)> clipCallback_;

    float zMin_    = 0;
    float zMax_    = 0;
    float dataMin_ = 0;   // true data min, used for clamping
    float dataMax_ = 0;

    static constexpr int kBins = 256;
    std::vector<uint32_t> bins_;
    uint32_t maxCount_   = 0;
    uint32_t totalValid_ = 0;

    HistoView* histoView_ = nullptr;
};
