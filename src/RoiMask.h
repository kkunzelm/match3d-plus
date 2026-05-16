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
    void clipToGradient(const ViffImage& img, float maxGrad);

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
