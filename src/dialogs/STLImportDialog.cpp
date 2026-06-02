#include "STLImportDialog.h"
#include "visualization3d/STLPreviewWidget.h"
#include "mesh3d/STLReader.h"
#include "mesh3d/MeshProjection.h"
#include "../AppSettings.h"

#include <cmath>
#include <algorithm>

#include <QBoxLayout>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QMessageBox>
#include <QPainter>
#include <QPushButton>
#include <QSlider>
#include <QSplitter>

// ── Constructor ──────────────────────────────────────────────────────────────

STLImportDialog::STLImportDialog(const QString& filePath,
                                 AppSettings* settings,
                                 const std::vector<OpenImageInfo>& openImages,
                                 QWidget* parent)
    : QDialog(parent)
    , m_filePath(filePath)
    , m_settings(settings)
    , m_openImages(openImages)
{
    setWindowTitle(tr("Import STL: %1").arg(QFileInfo(filePath).fileName()));
    setMinimumSize(1000, 700);
    resize(1200, 800);

    setupUI();
    loadSTL();
}

// ── UI Setup ─────────────────────────────────────────────────────────────────

void STLImportDialog::setupUI()
{
    auto* mainLayout = new QVBoxLayout(this);

    // ── Splitter: 3D preview | 2D preview ────────────────────────────────────
    auto* splitter = new QSplitter(Qt::Horizontal, this);

    // Left: 3D preview
    m_preview3D = new STLPreviewWidget(this);
    splitter->addWidget(m_preview3D);

    // Right: 2D preview + controls
    auto* rightPanel = new QWidget(this);
    auto* rightLayout = new QVBoxLayout(rightPanel);
    rightLayout->setContentsMargins(5, 5, 5, 5);

    // 2D projection preview
    auto* preview2DGroup = new QGroupBox(tr("2D Projection Preview"), rightPanel);
    auto* preview2DLayout = new QVBoxLayout(preview2DGroup);
    m_preview2D = new QLabel(preview2DGroup);
    m_preview2D->setMinimumSize(300, 300);
    m_preview2D->setAlignment(Qt::AlignCenter);
    m_preview2D->setStyleSheet("background-color: #333; border: 1px solid #666;");
    preview2DLayout->addWidget(m_preview2D);
    rightLayout->addWidget(preview2DGroup);

    // Projection info
    m_projectionInfo = new QLabel(tr("No projection yet"), rightPanel);
    rightLayout->addWidget(m_projectionInfo);

    rightLayout->addStretch();
    splitter->addWidget(rightPanel);

    splitter->setSizes({700, 400});
    mainLayout->addWidget(splitter, 1);

    // ── Controls below splitter ──────────────────────────────────────────────
    auto* controlsLayout = new QHBoxLayout();

    // Quick alignment buttons
    auto* quickGroup = new QGroupBox(tr("Quick Alignment"), this);
    auto* quickLayout = new QGridLayout(quickGroup);

    auto addQuickBtn = [&](const QString& text, int row, int col) {
        auto* btn = new QPushButton(text, quickGroup);
        btn->setFixedWidth(80);
        connect(btn, &QPushButton::clicked, this, &STLImportDialog::onQuickAlignClicked);
        quickLayout->addWidget(btn, row, col);
        return btn;
    };

    addQuickBtn(tr("Top"), 0, 0);
    addQuickBtn(tr("Bottom"), 0, 1);
    addQuickBtn(tr("Front"), 0, 2);
    addQuickBtn(tr("Back"), 0, 3);
    addQuickBtn(tr("90X"), 1, 0);
    addQuickBtn(tr("90Y"), 1, 1);
    addQuickBtn(tr("90Z"), 1, 2);
    addQuickBtn(tr("Reset"), 1, 3);

    controlsLayout->addWidget(quickGroup);

    // Fine rotation sliders
    auto* rotGroup = new QGroupBox(tr("Fine Rotation"), this);
    auto* rotLayout = new QFormLayout(rotGroup);

    auto addRotSlider = [&](const QString& label, QSlider*& slider, QLabel*& valueLabel) {
        auto* hbox = new QHBoxLayout();
        slider = new QSlider(Qt::Horizontal, rotGroup);
        slider->setRange(-180, 180);
        slider->setValue(0);
        slider->setFixedWidth(150);
        valueLabel = new QLabel("0°", rotGroup);
        valueLabel->setFixedWidth(50);
        hbox->addWidget(slider);
        hbox->addWidget(valueLabel);
        rotLayout->addRow(label, hbox);
        connect(slider, &QSlider::valueChanged, this, &STLImportDialog::onRotationSliderChanged);
    };

    addRotSlider(tr("X:"), m_sliderX, m_labelRotX);
    addRotSlider(tr("Y:"), m_sliderY, m_labelRotY);
    addRotSlider(tr("Z:"), m_sliderZ, m_labelRotZ);

    controlsLayout->addWidget(rotGroup);

    // Resolution settings
    auto* resGroup = new QGroupBox(tr("Projection"), this);
    auto* resLayout = new QFormLayout(resGroup);

    m_spinResolution = new QDoubleSpinBox(resGroup);
    m_spinResolution->setRange(0.001, 1.0);
    // Use global settings if available, otherwise default
    double defaultRes = m_settings ? static_cast<double>(m_settings->stlResolution) : 0.025;
    m_spinResolution->setValue(defaultRes);
    m_spinResolution->setSingleStep(0.005);
    m_spinResolution->setDecimals(3);
    m_spinResolution->setSuffix(tr(" mm/px"));
    connect(m_spinResolution, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &STLImportDialog::onResolutionChanged);
    resLayout->addRow(tr("Resolution:"), m_spinResolution);

    // "Copy resolution from" combo (only if there are open images)
    if (!m_openImages.empty()) {
        m_comboResFrom = new QComboBox(resGroup);
        m_comboResFrom->addItem(tr("-- Copy from --"));
        for (const auto& img : m_openImages) {
            // Show resolution as average of x and y pixel sizes
            // ViffImage.xPixelSize is already in mm (ViffReader converts to mm)
            float avgRes = (img.xPixelSize + img.yPixelSize) / 2.0f;
            m_comboResFrom->addItem(tr("%1 (%2 mm/px)")
                .arg(img.name)
                .arg(static_cast<double>(avgRes), 0, 'f', 3));
        }
        connect(m_comboResFrom, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, [this](int idx) {
            if (idx > 0 && idx <= static_cast<int>(m_openImages.size())) {
                const auto& img = m_openImages[static_cast<size_t>(idx - 1)];
                // Use average of x and y pixel sizes (already in mm)
                float avgRes = (img.xPixelSize + img.yPixelSize) / 2.0f;
                m_spinResolution->setValue(static_cast<double>(avgRes));
            }
        });
        resLayout->addRow(tr("Copy from:"), m_comboResFrom);
    }

    m_checkAutoSize = new QCheckBox(tr("Auto size"), resGroup);
    m_checkAutoSize->setChecked(true);
    resLayout->addRow(m_checkAutoSize);

    m_checkGraycast = new QCheckBox(tr("Graycast shading"), resGroup);
    m_checkGraycast->setChecked(true);
    connect(m_checkGraycast, &QCheckBox::toggled, this, &STLImportDialog::updateProjectionPreview);
    resLayout->addRow(m_checkGraycast);

    controlsLayout->addWidget(resGroup);

    // Mesh info
    auto* infoGroup = new QGroupBox(tr("Mesh Info"), this);
    auto* infoLayout = new QVBoxLayout(infoGroup);
    m_meshInfo = new QLabel(tr("Loading..."), infoGroup);
    m_meshInfo->setWordWrap(true);
    infoLayout->addWidget(m_meshInfo);
    controlsLayout->addWidget(infoGroup);

    controlsLayout->addStretch();
    mainLayout->addLayout(controlsLayout);

    // ── Dialog buttons ───────────────────────────────────────────────────────
    auto* buttonBox = new QDialogButtonBox(this);
    auto* cancelBtn = buttonBox->addButton(QDialogButtonBox::Cancel);
    m_btnImport = buttonBox->addButton(tr("Import"), QDialogButtonBox::AcceptRole);
    m_btnImport->setEnabled(false);

    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    connect(m_btnImport, &QPushButton::clicked, this, &STLImportDialog::onImportClicked);

    mainLayout->addWidget(buttonBox);

    // Connect 3D preview transform changes
    connect(m_preview3D, &STLPreviewWidget::transformChanged,
            this, &STLImportDialog::onTransformChanged);
}

// ── STL Loading ──────────────────────────────────────────────────────────────

void STLImportDialog::loadSTL()
{
    std::string errorMsg;
    m_mesh = mesh3d::readSTL(m_filePath.toStdString(), errorMsg);

    if (!m_mesh || !m_mesh->isValid()) {
        m_meshInfo->setText(tr("Error: %1").arg(QString::fromStdString(errorMsg)));
        return;
    }

    m_preview3D->setMesh(m_mesh);
    updateMeshInfo();
    m_btnImport->setEnabled(true);

    // Initial projection
    m_currentTransform = Eigen::Matrix4d::Identity();
    updateProjectionPreview();
}

void STLImportDialog::updateMeshInfo()
{
    if (!m_mesh) return;

    auto dims = m_mesh->dimensions();
    QString info = tr("Triangles: %1\nVertices: %2\n"
                      "Size: %3 × %4 × %5 mm")
        .arg(m_mesh->triangleCount)
        .arg(m_mesh->vertexCount)
        .arg(dims[0], 0, 'f', 2)
        .arg(dims[1], 0, 'f', 2)
        .arg(dims[2], 0, 'f', 2);

    m_meshInfo->setText(info);
}

// ── Slots ────────────────────────────────────────────────────────────────────

void STLImportDialog::onTransformChanged(const Eigen::Matrix4d& transform)
{
    m_currentTransform = transform;

    // Update slider values to match (extract Euler angles)
    double rx, ry, rz;
    m_preview3D->getRotation(rx, ry, rz);

    // Block signals to avoid feedback loop
    m_sliderX->blockSignals(true);
    m_sliderY->blockSignals(true);
    m_sliderZ->blockSignals(true);

    m_sliderX->setValue(static_cast<int>(rx));
    m_sliderY->setValue(static_cast<int>(ry));
    m_sliderZ->setValue(static_cast<int>(rz));

    m_sliderX->blockSignals(false);
    m_sliderY->blockSignals(false);
    m_sliderZ->blockSignals(false);

    m_labelRotX->setText(QString("%1°").arg(static_cast<int>(rx)));
    m_labelRotY->setText(QString("%1°").arg(static_cast<int>(ry)));
    m_labelRotZ->setText(QString("%1°").arg(static_cast<int>(rz)));

    updateProjectionPreview();
}

void STLImportDialog::onRotationSliderChanged()
{
    double rx = m_sliderX->value();
    double ry = m_sliderY->value();
    double rz = m_sliderZ->value();

    m_labelRotX->setText(QString("%1°").arg(static_cast<int>(rx)));
    m_labelRotY->setText(QString("%1°").arg(static_cast<int>(ry)));
    m_labelRotZ->setText(QString("%1°").arg(static_cast<int>(rz)));

    m_preview3D->setRotation(rx, ry, rz);
}

void STLImportDialog::onResolutionChanged()
{
    updateProjectionPreview();
}

void STLImportDialog::onQuickAlignClicked()
{
    auto* btn = qobject_cast<QPushButton*>(sender());
    if (!btn) return;

    // Use property instead of translated text for reliable comparison
    QString text = btn->text();

    // Compare against both translated and untranslated strings
    if (text == tr("Top") || text == "Top")           m_preview3D->setViewTop();
    else if (text == tr("Bottom") || text == "Bottom") m_preview3D->setViewBottom();
    else if (text == tr("Front") || text == "Front")   m_preview3D->setViewFront();
    else if (text == tr("Back") || text == "Back")     m_preview3D->setViewBack();
    else if (text == tr("90X") || text == "90X")       m_preview3D->rotateX90();
    else if (text == tr("90Y") || text == "90Y")       m_preview3D->rotateY90();
    else if (text == tr("90Z") || text == "90Z")       m_preview3D->rotateZ90();
    else if (text == tr("Reset") || text == "Reset")   m_preview3D->resetTransform();
}

void STLImportDialog::updateProjectionPreview()
{
    if (!m_mesh || !m_mesh->isValid()) return;

    // Create projection with current settings
    mesh3d::ProjectionParams params;
    params.resolution = m_spinResolution->value();
    params.autoSize = m_checkAutoSize->isChecked();

    auto result = mesh3d::projectToHeightmap(*m_mesh, m_currentTransform, params);

    // Update info label (ViffImage uses cols=width, rows=height)
    m_projectionInfo->setText(tr("Size: %1 × %2 px | Coverage: %3%")
        .arg(result.image.cols)
        .arg(result.image.rows)
        .arg(result.coveragePercent, 0, 'f', 1));

    // Find Z range for normalization
    float zMin = static_cast<float>(result.zMin);
    float zMax = static_cast<float>(result.zMax);

    // Render with selected style
    QImage img = m_checkGraycast->isChecked()
        ? renderGraycast(result.image, zMin, zMax)
        : renderLinear(result.image, zMin, zMax);

    // Scale for preview
    int previewWidth = std::min(300, static_cast<int>(result.image.cols));
    int previewHeight = std::min(300, static_cast<int>(result.image.rows));

    QPixmap pixmap = QPixmap::fromImage(img.scaled(
        previewWidth, previewHeight, Qt::KeepAspectRatio, Qt::SmoothTransformation));

    m_preview2D->setPixmap(pixmap);
}

// ── Graycast rendering (Sobel-based shaded relief) ──────────────────────────

QImage STLImportDialog::renderGraycast(const ViffImage& img, float zMin, float zMax)
{
    // Sobel kernels for gradient computation
    static constexpr int kx[3][3] = {{-1, 0, 1}, {-2, 0, 2}, {-1, 0, 1}};
    static constexpr int ky[3][3] = {{ 1, 2, 1}, { 0, 0, 0}, {-1,-2,-1}};

    const int w = static_cast<int>(img.cols);
    const int h = static_cast<int>(img.rows);

    // Reference pixel size for gradient normalization
    const float zRange = zMax - zMin;
    const double refPxSize = (zRange > 0.0f)
        ? (zRange / std::max(w, h))
        : 1.0;

    QImage result(w, h, QImage::Format_RGB32);

    for (int r = 0; r < h; ++r) {
        QRgb* line = reinterpret_cast<QRgb*>(result.scanLine(r));
        for (int c = 0; c < w; ++c) {
            size_t idx = static_cast<size_t>(r) * w + c;
            float z = img.data[idx];

            // Invalid pixel (no data)
            if (z == 0.0f) {
                line[c] = qRgb(0, 0, 0);
                continue;
            }

            // 3×3 Sobel convolution
            double gx = 0.0, gy = 0.0;
            for (int dr = -1; dr <= 1; ++dr) {
                for (int dc = -1; dc <= 1; ++dc) {
                    int nr = std::clamp(r + dr, 0, h - 1);
                    int nc = std::clamp(c + dc, 0, w - 1);
                    size_t nidx = static_cast<size_t>(nr) * w + nc;
                    double v = img.data[nidx];
                    gx += kx[dr + 1][dc + 1] * v;
                    gy += ky[dr + 1][dc + 1] * v;
                }
            }
            gx /= (8.0 * refPxSize);
            gy /= (8.0 * refPxSize);

            // Lambertian shading with zenith light
            const double theta = std::atan(std::sqrt(gx * gx + gy * gy));
            const float cos_v = static_cast<float>(std::cos(theta));
            // Invert: flat surfaces dark, steep slopes bright (dental convention)
            int gray = (cos_v < 1.0f) ? static_cast<int>((1.0f - cos_v) * 255.0f) : 0;
            // Alternative: int gray = static_cast<int>(cos_v * 255.0f);  // flat=bright

            line[c] = qRgb(gray, gray, gray);
        }
    }

    return result;
}

QImage STLImportDialog::renderLinear(const ViffImage& img, float zMin, float zMax)
{
    const int w = static_cast<int>(img.cols);
    const int h = static_cast<int>(img.rows);
    const float zRange = zMax - zMin;

    QImage result(w, h, QImage::Format_RGB32);

    for (int r = 0; r < h; ++r) {
        QRgb* line = reinterpret_cast<QRgb*>(result.scanLine(r));
        for (int c = 0; c < w; ++c) {
            size_t idx = static_cast<size_t>(r) * w + c;
            float z = img.data[idx];

            uint8_t gray;
            if (z == 0.0f) {  // No data
                gray = 0;
            } else if (zRange > 1e-6f) {
                float norm = (z - zMin) / zRange;
                gray = static_cast<uint8_t>(std::clamp(norm * 255.0f, 0.0f, 255.0f));
            } else {
                gray = 128;
            }
            line[c] = qRgb(gray, gray, gray);
        }
    }

    return result;
}

void STLImportDialog::onImportClicked()
{
    createProjection();
    accept();
}

void STLImportDialog::createProjection()
{
    if (!m_mesh || !m_mesh->isValid()) return;

    mesh3d::ProjectionParams params;
    params.resolution = m_spinResolution->value();
    params.autoSize = m_checkAutoSize->isChecked();

    auto result = mesh3d::projectToHeightmap(*m_mesh, m_currentTransform, params);
    m_result = result.image;
}
