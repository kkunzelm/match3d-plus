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

// ── Surface Fitting ──────────────────────────────────────────────────────────

ImageProcessor::PlaneFit ImageProcessor::fitPlane(const ViffImage& img, const RoiMask* roi) {
    PlaneFit result;

    // Collect all valid selected points in world coordinates
    std::vector<double> xPts, yPts, zPts;
    xPts.reserve(img.rows * img.cols / 4);
    yPts.reserve(img.rows * img.cols / 4);
    zPts.reserve(img.rows * img.cols / 4);

    for (uint32_t r = 0; r < img.rows; ++r) {
        for (uint32_t c = 0; c < img.cols; ++c) {
            if (!img.isValid(r, c)) continue;
            if (roi && !roi->isSelected(r, c)) continue;
            // Convert to world coordinates (pixel center)
            const double x = static_cast<double>(c) * img.xPixelSize;
            const double y = static_cast<double>(r) * img.yPixelSize;
            const double z = static_cast<double>(img.at(r, c));
            xPts.push_back(x);
            yPts.push_back(y);
            zPts.push_back(z);
        }
    }

    result.pointCount = static_cast<uint32_t>(xPts.size());
    if (result.pointCount < 3) {
        return result;  // Need at least 3 points for a plane
    }

    // Build normal equations for least squares plane fit: z = Ax + By + C
    // System: M * [A, B, C]^T = b
    // where M is 3x3 and b is 3x1
    double m[3][3] = {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}};
    double b[3] = {0, 0, 0};

    for (size_t i = 0; i < xPts.size(); ++i) {
        const double x = xPts[i];
        const double y = yPts[i];
        const double z = zPts[i];

        m[0][0] += x * x;
        m[0][1] += x * y;
        m[0][2] += x;
        m[1][0] += x * y;
        m[1][1] += y * y;
        m[1][2] += y;
        m[2][0] += x;
        m[2][1] += y;
        m[2][2] += 1.0;

        b[0] += x * z;
        b[1] += y * z;
        b[2] += z;
    }

    // Solve 3x3 system using Cramer's rule (simple for 3x3)
    auto det3 = [](double a[3][3]) -> double {
        return a[0][0] * (a[1][1] * a[2][2] - a[1][2] * a[2][1])
             - a[0][1] * (a[1][0] * a[2][2] - a[1][2] * a[2][0])
             + a[0][2] * (a[1][0] * a[2][1] - a[1][1] * a[2][0]);
    };

    const double detM = det3(m);
    if (std::abs(detM) < 1e-15) {
        return result;  // Singular matrix - points are collinear
    }

    // Solve for A (replace column 0 with b)
    double mA[3][3];
    for (int i = 0; i < 3; ++i) {
        mA[i][0] = b[i];
        mA[i][1] = m[i][1];
        mA[i][2] = m[i][2];
    }
    result.A = det3(mA) / detM;

    // Solve for B (replace column 1 with b)
    double mB[3][3];
    for (int i = 0; i < 3; ++i) {
        mB[i][0] = m[i][0];
        mB[i][1] = b[i];
        mB[i][2] = m[i][2];
    }
    result.B = det3(mB) / detM;

    // Solve for C (replace column 2 with b)
    double mC[3][3];
    for (int i = 0; i < 3; ++i) {
        mC[i][0] = m[i][0];
        mC[i][1] = m[i][1];
        mC[i][2] = b[i];
    }
    result.C = det3(mC) / detM;

    // Calculate RMS error
    double sumSqErr = 0;
    for (size_t i = 0; i < xPts.size(); ++i) {
        const double zFit = result.A * xPts[i] + result.B * yPts[i] + result.C;
        const double err = zPts[i] - zFit;
        sumSqErr += err * err;
    }
    result.rmsError = std::sqrt(sumSqErr / xPts.size());
    result.valid = true;

    return result;
}

ViffImage ImageProcessor::subtractPlane(const ViffImage& img, const PlaneFit& fit) {
    ViffImage out = img;
    out.isDiffImage = true;  // Result can have negative values

    for (uint32_t r = 0; r < img.rows; ++r) {
        for (uint32_t c = 0; c < img.cols; ++c) {
            const size_t idx = r * img.cols + c;
            if (!img.isValid(r, c)) {
                out.data[idx] = 0.0f;
                continue;
            }
            // Calculate plane value at this pixel
            const double x = static_cast<double>(c) * img.xPixelSize;
            const double y = static_cast<double>(r) * img.yPixelSize;
            const double zPlane = fit.A * x + fit.B * y + fit.C;
            out.data[idx] = img.data[idx] - static_cast<float>(zPlane);
        }
    }

    return out;
}

QString ImageProcessor::formatPlaneFit(const PlaneFit& fit, const QString& imageLabel) {
    const QString date = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm");

    if (!fit.valid) {
        return QString("# match3d plane fit\n"
                       "# Image: %1\n"
                       "# Date:  %2\n"
                       "# FAILED: Not enough points (need >= 3)\n")
                   .arg(imageLabel).arg(date);
    }

    // Convert slope to angle (degrees from horizontal)
    const double slopeX = std::atan(fit.A) * 180.0 / M_PI;
    const double slopeY = std::atan(fit.B) * 180.0 / M_PI;

    return QString("# match3d plane fit\n"
                   "# Image: %1\n"
                   "# Date:  %2\n"
                   "# Plane equation: z = A*x + B*y + C\n"
                   "Points         = %3\n"
                   "A              = %4\n"
                   "B              = %5\n"
                   "C              = %6 mm\n"
                   "Slope X        = %7 deg\n"
                   "Slope Y        = %8 deg\n"
                   "RMS Error      = %9 mm\n")
               .arg(imageLabel).arg(date)
               .arg(fit.pointCount)
               .arg(fit.A, 0, 'e', 6)
               .arg(fit.B, 0, 'e', 6)
               .arg(fit.C, 0, 'f', 6)
               .arg(slopeX, 0, 'f', 4)
               .arg(slopeY, 0, 'f', 4)
               .arg(fit.rmsError, 0, 'f', 6);
}

ImageProcessor::SphereFit ImageProcessor::fitSphere(const ViffImage& img, const RoiMask* roi) {
    SphereFit result;

    // Collect all valid selected points in world coordinates
    std::vector<double> xPts, yPts, zPts;
    xPts.reserve(img.rows * img.cols / 4);
    yPts.reserve(img.rows * img.cols / 4);
    zPts.reserve(img.rows * img.cols / 4);

    for (uint32_t r = 0; r < img.rows; ++r) {
        for (uint32_t c = 0; c < img.cols; ++c) {
            if (!img.isValid(r, c)) continue;
            if (roi && !roi->isSelected(r, c)) continue;
            const double x = static_cast<double>(c) * img.xPixelSize;
            const double y = static_cast<double>(r) * img.yPixelSize;
            const double z = static_cast<double>(img.at(r, c));
            xPts.push_back(x);
            yPts.push_back(y);
            zPts.push_back(z);
        }
    }

    const size_t n = xPts.size();
    result.pointCount = static_cast<uint32_t>(n);
    if (n < 4) {
        return result;  // Need at least 4 points for a sphere
    }

    // Initial estimate: centroid and average distance
    double xSum = 0, ySum = 0, zSum = 0;
    for (size_t i = 0; i < n; ++i) {
        xSum += xPts[i];
        ySum += yPts[i];
        zSum += zPts[i];
    }
    double h = xSum / n;
    double k = ySum / n;
    double l = zSum / n;

    // Initial radius estimate
    double radius = 0;
    for (size_t i = 0; i < n; ++i) {
        const double dx = xPts[i] - h;
        const double dy = yPts[i] - k;
        const double dz = zPts[i] - l;
        radius += std::sqrt(dx * dx + dy * dy + dz * dz);
    }
    radius /= n;

    // Gauss-Newton iteration
    const int maxIter = 100;
    const double tol = 1e-10;
    double gOld = 1.0, gNew = 100.0;
    std::vector<double> r_i(n);

    int iter = 0;
    while (std::abs(gNew - gOld) > tol && iter < maxIter) {
        ++iter;
        gOld = gNew;

        // Build Jacobian J (n x 4) and residual d (n x 1)
        // Residual: d_i = r_i - radius, where r_i = distance from point i to center
        // Jacobian: J_i = [-(x_i-h)/r_i, -(y_i-k)/r_i, -(z_i-l)/r_i, -1]

        // Using normal equations: (J^T * J) * delta = J^T * (-d)
        double JtJ[4][4] = {{0,0,0,0}, {0,0,0,0}, {0,0,0,0}, {0,0,0,0}};
        double Jtd[4] = {0, 0, 0, 0};

        for (size_t i = 0; i < n; ++i) {
            const double dx = xPts[i] - h;
            const double dy = yPts[i] - k;
            const double dz = zPts[i] - l;
            r_i[i] = std::sqrt(dx * dx + dy * dy + dz * dz);

            if (r_i[i] < 1e-12) continue;  // Skip degenerate case

            const double d_i = r_i[i] - radius;
            const double j0 = -dx / r_i[i];
            const double j1 = -dy / r_i[i];
            const double j2 = -dz / r_i[i];
            const double j3 = -1.0;

            // Build J^T * J
            JtJ[0][0] += j0 * j0;  JtJ[0][1] += j0 * j1;  JtJ[0][2] += j0 * j2;  JtJ[0][3] += j0 * j3;
            JtJ[1][0] += j1 * j0;  JtJ[1][1] += j1 * j1;  JtJ[1][2] += j1 * j2;  JtJ[1][3] += j1 * j3;
            JtJ[2][0] += j2 * j0;  JtJ[2][1] += j2 * j1;  JtJ[2][2] += j2 * j2;  JtJ[2][3] += j2 * j3;
            JtJ[3][0] += j3 * j0;  JtJ[3][1] += j3 * j1;  JtJ[3][2] += j3 * j2;  JtJ[3][3] += j3 * j3;

            // Build J^T * (-d)
            Jtd[0] += j0 * d_i;
            Jtd[1] += j1 * d_i;
            Jtd[2] += j2 * d_i;
            Jtd[3] += j3 * d_i;
        }

        // Solve 4x4 system using Gaussian elimination with partial pivoting
        double aug[4][5];
        for (int i = 0; i < 4; ++i) {
            for (int j = 0; j < 4; ++j)
                aug[i][j] = JtJ[i][j];
            aug[i][4] = Jtd[i];
        }

        // Forward elimination with pivoting
        for (int col = 0; col < 4; ++col) {
            // Find pivot
            int maxRow = col;
            for (int row = col + 1; row < 4; ++row)
                if (std::abs(aug[row][col]) > std::abs(aug[maxRow][col]))
                    maxRow = row;
            // Swap rows
            for (int j = 0; j < 5; ++j)
                std::swap(aug[col][j], aug[maxRow][j]);

            if (std::abs(aug[col][col]) < 1e-15) continue;  // Singular

            // Eliminate below
            for (int row = col + 1; row < 4; ++row) {
                const double f = aug[row][col] / aug[col][col];
                for (int j = col; j < 5; ++j)
                    aug[row][j] -= f * aug[col][j];
            }
        }

        // Back substitution
        double delta[4] = {0, 0, 0, 0};
        for (int row = 3; row >= 0; --row) {
            double sum = aug[row][4];
            for (int j = row + 1; j < 4; ++j)
                sum -= aug[row][j] * delta[j];
            if (std::abs(aug[row][row]) > 1e-15)
                delta[row] = sum / aug[row][row];
        }

        // Update parameters
        h += delta[0];
        k += delta[1];
        l += delta[2];
        radius += delta[3];

        // Compute gradient norm for convergence check
        gNew = 0;
        for (int i = 0; i < 4; ++i)
            gNew += std::abs(Jtd[i]);
    }

    result.h = h;
    result.k = k;
    result.l = l;
    result.radius = std::abs(radius);
    result.iterations = iter;

    // Calculate RMS error
    double sumSqErr = 0;
    for (size_t i = 0; i < n; ++i) {
        const double dx = xPts[i] - h;
        const double dy = yPts[i] - k;
        const double dz = zPts[i] - l;
        const double dist = std::sqrt(dx * dx + dy * dy + dz * dz);
        const double err = dist - result.radius;
        sumSqErr += err * err;
    }
    result.rmsError = std::sqrt(sumSqErr / n);

    // Auto-detect orientation: check if center z is above or below average z
    // If l > zMean, sphere is convex (looking at it from above)
    const double zMean = zSum / n;
    result.convex = (l > zMean);

    result.valid = true;
    return result;
}

ViffImage ImageProcessor::subtractSphere(const ViffImage& img, const SphereFit& fit) {
    ViffImage out = img;
    out.isDiffImage = true;  // Result can have negative values

    for (uint32_t r = 0; r < img.rows; ++r) {
        for (uint32_t c = 0; c < img.cols; ++c) {
            const size_t idx = r * img.cols + c;
            if (!img.isValid(r, c)) {
                out.data[idx] = 0.0f;
                continue;
            }

            // Calculate sphere value at this pixel
            const double x = static_cast<double>(c) * img.xPixelSize;
            const double y = static_cast<double>(r) * img.yPixelSize;
            const double dx = x - fit.h;
            const double dy = y - fit.k;
            const double xyDistSq = dx * dx + dy * dy;

            // Check if point is within sphere's xy projection
            if (xyDistSq >= fit.radius * fit.radius) {
                // Outside sphere projection - just use original value
                // (could also set to 0 or NaN depending on desired behavior)
                continue;
            }

            // z = l ± sqrt(r² - (x-h)² - (y-k)²)
            // Use + for convex (sphere above), - for concave (sphere below)
            const double zSphere = fit.l + (fit.convex ? 1.0 : -1.0) *
                                   std::sqrt(fit.radius * fit.radius - xyDistSq);
            out.data[idx] = img.data[idx] - static_cast<float>(zSphere);
        }
    }

    return out;
}

QString ImageProcessor::formatSphereFit(const SphereFit& fit, const QString& imageLabel) {
    const QString date = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm");

    if (!fit.valid) {
        return QString("# match3d sphere fit\n"
                       "# Image: %1\n"
                       "# Date:  %2\n"
                       "# FAILED: Not enough points (need >= 4)\n")
                   .arg(imageLabel).arg(date);
    }

    return QString("# match3d sphere fit\n"
                   "# Image: %1\n"
                   "# Date:  %2\n"
                   "# Sphere equation: (x-h)² + (y-k)² + (z-l)² = r²\n"
                   "Points         = %3\n"
                   "Center h (X)   = %4 mm\n"
                   "Center k (Y)   = %5 mm\n"
                   "Center l (Z)   = %6 mm\n"
                   "Radius         = %7 mm\n"
                   "Diameter       = %8 mm\n"
                   "Orientation    = %9\n"
                   "Iterations     = %10\n"
                   "RMS Error      = %11 mm\n")
               .arg(imageLabel).arg(date)
               .arg(fit.pointCount)
               .arg(fit.h, 0, 'f', 6)
               .arg(fit.k, 0, 'f', 6)
               .arg(fit.l, 0, 'f', 6)
               .arg(fit.radius, 0, 'f', 6)
               .arg(fit.radius * 2.0, 0, 'f', 6)
               .arg(fit.convex ? "Convex (above)" : "Concave (below)")
               .arg(fit.iterations)
               .arg(fit.rmsError, 0, 'f', 6);
}
