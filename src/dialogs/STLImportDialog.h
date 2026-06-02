#pragma once

/**
 * @file STLImportDialog.h
 * @brief Dialog for importing STL files with interactive orientation
 *
 * Provides:
 * - 3D preview with object rotation
 * - Live 2D projection preview
 * - Quick alignment buttons
 * - Fine rotation sliders
 * - Resolution settings
 *
 * Cross-platform compatible (Linux, Windows).
 */

#include "mesh3d/MeshData.h"
#include "io/ViffReader.h"

#include <QDialog>
#include <QImage>
#include <Eigen/Core>
#include <memory>
#include <vector>

class STLPreviewWidget;
class QLabel;
class QSlider;
class QDoubleSpinBox;
class QCheckBox;
class QPushButton;
class QComboBox;

struct AppSettings;

/// Info about an open image for resolution selection
struct OpenImageInfo {
    QString name;
    float xPixelSize;  // mm per pixel
    float yPixelSize;
};

/**
 * @brief Dialog for STL import with interactive 3D orientation
 *
 * Usage:
 * @code
 * STLImportDialog dlg(filePath, parentWidget);
 * if (dlg.exec() == QDialog::Accepted) {
 *     ViffImage img = dlg.getProjectedImage();
 *     // Use the projected heightmap...
 * }
 * @endcode
 */
class STLImportDialog : public QDialog {
    Q_OBJECT

public:
    /**
     * @brief Construct dialog and load STL file
     * @param filePath Path to STL file
     * @param settings Global settings for default resolution (can be nullptr)
     * @param openImages List of open images for "copy resolution from" feature
     * @param parent Parent widget
     */
    explicit STLImportDialog(const QString& filePath,
                             AppSettings* settings = nullptr,
                             const std::vector<OpenImageInfo>& openImages = {},
                             QWidget* parent = nullptr);
    ~STLImportDialog() override = default;

    /// Get the projected heightmap (valid after accept())
    ViffImage getProjectedImage() const { return m_result; }

    /// Get source file path
    QString getSourcePath() const { return m_filePath; }

    /// Check if STL was loaded successfully
    bool isValid() const { return m_mesh && m_mesh->isValid(); }

private slots:
    void onTransformChanged(const Eigen::Matrix4d& transform);
    void onRotationSliderChanged();
    void onResolutionChanged();
    void onQuickAlignClicked();
    void updateProjectionPreview();
    void onImportClicked();

private:
    void setupUI();
    void loadSTL();
    void updateMeshInfo();
    void createProjection();

    // ── Data ─────────────────────────────────────────────────────────────────
    QString m_filePath;
    std::shared_ptr<mesh3d::MeshData> m_mesh;
    ViffImage m_result;
    Eigen::Matrix4d m_currentTransform = Eigen::Matrix4d::Identity();

    // ── Widgets ──────────────────────────────────────────────────────────────
    STLPreviewWidget* m_preview3D = nullptr;
    QLabel* m_preview2D = nullptr;
    QLabel* m_meshInfo = nullptr;
    QLabel* m_projectionInfo = nullptr;

    // Rotation sliders
    QSlider* m_sliderX = nullptr;
    QSlider* m_sliderY = nullptr;
    QSlider* m_sliderZ = nullptr;
    QLabel* m_labelRotX = nullptr;
    QLabel* m_labelRotY = nullptr;
    QLabel* m_labelRotZ = nullptr;

    // Resolution
    QDoubleSpinBox* m_spinResolution = nullptr;
    QCheckBox* m_checkAutoSize = nullptr;
    QCheckBox* m_checkGraycast = nullptr;
    QComboBox* m_comboResFrom = nullptr;

    // Buttons
    QPushButton* m_btnImport = nullptr;

    // Settings and open images
    AppSettings* m_settings = nullptr;
    std::vector<OpenImageInfo> m_openImages;

    // ── Preview rendering helpers ───────────────────────────────────────────
    QImage renderGraycast(const ViffImage& img, float zMin, float zMax);
    QImage renderLinear(const ViffImage& img, float zMin, float zMax);
};
