#include "ImageProcessor.h"
#include "RoiMask.h"
#include "io/ViffReader.h"

#include <QDateTime>
#include <QString>

#include <algorithm>
#include <cmath>
#include <limits>
#include <random>
#include <vector>

// ── Helpers ────────────────────────────────────────────────────────────────────

static inline bool valid(const ViffImage& img, int r, int c) {
    if (r < 0 || r >= static_cast<int>(img.rows)) return false;
    if (c < 0 || c >= static_cast<int>(img.cols)) return false;
    return img.isValid(static_cast<uint32_t>(r), static_cast<uint32_t>(c));
}

static inline float pix(const ViffImage& img, int r, int c) {
    return img.at(static_cast<uint32_t>(r), static_cast<uint32_t>(c));
}

// Collect valid neighbor values in [r±half, c±half] window into buf.
static void gatherNeighbors(const ViffImage& img, int r, int c, int half,
                             std::vector<float>& buf) {
    buf.clear();
    for (int dr = -half; dr <= half; ++dr)
        for (int dc = -half; dc <= half; ++dc)
            if (valid(img, r + dr, c + dc))
                buf.push_back(pix(img, r + dr, c + dc));
}

// ── Filters ───────────────────────────────────────────────────────────────────

void ImageProcessor::medianFilter(ViffImage& img, int kernelSize) {
    const int half = kernelSize / 2;
    ViffImage out = img;
    std::vector<float> buf;
    buf.reserve(kernelSize * kernelSize);

    for (int r = 0; r < static_cast<int>(img.rows); ++r) {
        for (int c = 0; c < static_cast<int>(img.cols); ++c) {
            if (!valid(img, r, c)) continue;
            gatherNeighbors(img, r, c, half, buf);
            if (buf.empty()) continue;
            const size_t mid = buf.size() / 2;
            std::nth_element(buf.begin(), buf.begin() + static_cast<ptrdiff_t>(mid), buf.end());
            out.data[static_cast<uint32_t>(r) * img.cols + static_cast<uint32_t>(c)] = buf[mid];
        }
    }
    img.data = std::move(out.data);
}

void ImageProcessor::completeFilter(ViffImage& img, int kernelSize) {
    const int half = kernelSize / 2;
    ViffImage out = img;
    std::vector<float> buf;
    buf.reserve(kernelSize * kernelSize);

    for (int r = 0; r < static_cast<int>(img.rows); ++r) {
        for (int c = 0; c < static_cast<int>(img.cols); ++c) {
            if (valid(img, r, c)) continue;  // only fill invalid pixels
            gatherNeighbors(img, r, c, half, buf);
            if (buf.empty()) continue;
            double sum = 0;
            for (float v : buf) sum += v;
            out.data[static_cast<uint32_t>(r) * img.cols + static_cast<uint32_t>(c)] =
                static_cast<float>(sum / buf.size());
        }
    }
    img.data = std::move(out.data);
}

void ImageProcessor::clipOutliers(ViffImage& img, int kernelSize, float maxDev) {
    const int half = kernelSize / 2;
    ViffImage out = img;
    std::vector<float> buf;
    buf.reserve(kernelSize * kernelSize);

    for (int r = 0; r < static_cast<int>(img.rows); ++r) {
        for (int c = 0; c < static_cast<int>(img.cols); ++c) {
            if (!valid(img, r, c)) continue;
            const float center = pix(img, r, c);
            // Gather neighbors excluding center
            buf.clear();
            for (int dr = -half; dr <= half; ++dr)
                for (int dc = -half; dc <= half; ++dc)
                    if ((dr != 0 || dc != 0) && valid(img, r + dr, c + dc))
                        buf.push_back(pix(img, r + dr, c + dc));
            if (buf.empty()) continue;
            double sum = 0;
            for (float v : buf) sum += v;
            const float mean = static_cast<float>(sum / buf.size());
            if (std::abs(center - mean) > maxDev)
                out.data[static_cast<uint32_t>(r) * img.cols + static_cast<uint32_t>(c)] = 0.0f;
        }
    }
    img.data = std::move(out.data);
}

void ImageProcessor::thinOut3x3(ViffImage& img, int minNeighbors) {
    ViffImage out = img;
    static const int dr8[] = {-1,-1,-1, 0, 0, 1, 1, 1};
    static const int dc8[] = {-1, 0, 1,-1, 1,-1, 0, 1};

    for (int r = 0; r < static_cast<int>(img.rows); ++r) {
        for (int c = 0; c < static_cast<int>(img.cols); ++c) {
            if (!valid(img, r, c)) continue;
            int count = 0;
            for (int i = 0; i < 8; ++i)
                if (valid(img, r + dr8[i], c + dc8[i])) ++count;
            if (count < minNeighbors)
                out.data[static_cast<uint32_t>(r) * img.cols + static_cast<uint32_t>(c)] = 0.0f;
        }
    }
    img.data = std::move(out.data);
}

void ImageProcessor::addGaussianNoise(ViffImage& img, const RoiMask* roi, float sigma) {
    std::mt19937 rng(std::random_device{}());
    std::normal_distribution<float> dist(0.0f, sigma);

    for (uint32_t r = 0; r < img.rows; ++r) {
        for (uint32_t c = 0; c < img.cols; ++c) {
            if (!img.isValid(r, c)) continue;
            if (roi && !roi->isSelected(r, c)) continue;
            img.data[r * img.cols + c] += dist(rng);
        }
    }
}

// ── Transforms ────────────────────────────────────────────────────────────────

void ImageProcessor::mirrorX(ViffImage& img) {
    for (uint32_t r = 0; r < img.rows; ++r) {
        float* row = img.data.data() + r * img.cols;
        std::reverse(row, row + img.cols);
    }
}

void ImageProcessor::shift(ViffImage& img, int dcol, int drow, float dz) {
    // Spatial shift: move pixels by (dcol, drow), vacated positions become 0.
    if (dcol != 0 || drow != 0) {
        ViffImage out = img;
        std::fill(out.data.begin(), out.data.end(), 0.0f);
        for (int r = 0; r < static_cast<int>(img.rows); ++r) {
            const int nr = r + drow;
            if (nr < 0 || nr >= static_cast<int>(img.rows)) continue;
            for (int c = 0; c < static_cast<int>(img.cols); ++c) {
                const int nc = c + dcol;
                if (nc < 0 || nc >= static_cast<int>(img.cols)) continue;
                out.data[static_cast<uint32_t>(nr) * img.cols + static_cast<uint32_t>(nc)] =
                    img.data[static_cast<uint32_t>(r)  * img.cols + static_cast<uint32_t>(c)];
            }
        }
        img.data = std::move(out.data);
    }
    // Z offset: add to all valid pixels
    if (dz != 0.0f) {
        for (uint32_t i = 0; i < img.rows * img.cols; ++i)
            if (img.data[i] > 0.0f && std::isfinite(img.data[i]))
                img.data[i] += dz;
    }
}

void ImageProcessor::scaleZ(ViffImage& img, const RoiMask* roi, float sx, float sy, float sz) {
    if (sx != 1.0f) img.xPixelSize *= sx;
    if (sy != 1.0f) img.yPixelSize *= sy;
    if (sz != 1.0f) {
        for (uint32_t r = 0; r < img.rows; ++r)
            for (uint32_t c = 0; c < img.cols; ++c) {
                if (!img.isValid(r, c)) continue;
                if (roi && !roi->isSelected(r, c)) continue;
                img.data[r * img.cols + c] *= sz;
            }
    }
}

// ── Z-range operations ────────────────────────────────────────────────────────

void ImageProcessor::subtractGlobalMin(ViffImage& img, const RoiMask* roi) {
    float zMin = std::numeric_limits<float>::max();
    for (uint32_t r = 0; r < img.rows; ++r)
        for (uint32_t c = 0; c < img.cols; ++c) {
            if (!img.isValid(r, c)) continue;
            if (roi && !roi->isSelected(r, c)) continue;
            zMin = std::min(zMin, img.at(r, c));
        }
    if (zMin == std::numeric_limits<float>::max() || zMin == 0.0f) return;
    for (uint32_t i = 0; i < img.rows * img.cols; ++i)
        if (img.data[i] > 0.0f && std::isfinite(img.data[i]))
            img.data[i] -= zMin;
}

void ImageProcessor::subtractPoint0(ViffImage& img) {
    // Find first valid pixel and use its value as offset
    float ref = 0.0f;
    bool found = false;
    for (uint32_t r = 0; r < img.rows && !found; ++r)
        for (uint32_t c = 0; c < img.cols && !found; ++c)
            if (img.isValid(r, c)) { ref = img.at(r, c); found = true; }
    if (!found || ref == 0.0f) return;
    for (uint32_t i = 0; i < img.rows * img.cols; ++i)
        if (img.data[i] > 0.0f && std::isfinite(img.data[i]))
            img.data[i] -= ref;
}

// ── Statistics ────────────────────────────────────────────────────────────────

ImageProcessor::Stats ImageProcessor::computeStats(const ViffImage& img,
                                                    const RoiMask* roi) {
    std::vector<float> vals;
    vals.reserve(img.rows * img.cols / 2);

    for (uint32_t r = 0; r < img.rows; ++r)
        for (uint32_t c = 0; c < img.cols; ++c) {
            if (!img.isValid(r, c)) continue;
            if (roi && !roi->isSelected(r, c)) continue;
            vals.push_back(img.at(r, c));
        }

    Stats s;
    s.validCount = static_cast<uint32_t>(vals.size());
    if (vals.empty()) return s;

    std::sort(vals.begin(), vals.end());
    s.min = vals.front();
    s.max = vals.back();
    auto pct = [&](int p) -> float {
        return vals[vals.size() * static_cast<size_t>(p) / 100];
    };
    s.q02 = pct(2);
    s.q05 = pct(5);
    s.q10 = pct(10);
    s.q50 = pct(50);
    s.q90 = pct(90);
    s.q95 = pct(95);
    s.q98 = pct(98);

    double sum = 0;
    for (float v : vals) sum += v;
    s.mean = static_cast<float>(sum / vals.size());

    double var = 0;
    for (float v : vals) var += static_cast<double>(v - s.mean) * (v - s.mean);
    s.stddev = static_cast<float>(std::sqrt(var / vals.size()));

    return s;
}

QString ImageProcessor::formatStats(const Stats& s, const QString& imageLabel) {
    const QString date = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm");
    auto row = [](const char* key, double val, int decimals = 4) -> QString {
        return QString("%1 = %2\n").arg(key, -14).arg(val, 0, 'f', decimals);
    };
    return QString("# match3d statistics\n"
                   "# Image: %1\n"
                   "# Date:  %2\n"
                   "ValidPixels    = %3\n")
               .arg(imageLabel).arg(date).arg(s.validCount)
           + row("Min",    s.min)
           + row("Max",    s.max)
           + row("Mean",   s.mean)
           + row("StdDev", s.stddev)
           + row("Q02",    s.q02)
           + row("Q05",    s.q05)
           + row("Q10",    s.q10)
           + row("Q50",    s.q50)
           + row("Q90",    s.q90)
           + row("Q95",    s.q95)
           + row("Q98",    s.q98);
}
