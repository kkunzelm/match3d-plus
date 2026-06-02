#pragma once

/**
 * @file STLPreviewWidget.h
 * @brief VTK-based 3D preview widget for STL import
 *
 * Provides interactive 3D visualization with OBJECT rotation (not camera).
 * The user rotates the mesh to orient it for projection onto the XY plane.
 * The transformation matrix can be extracted for the projection step.
 *
 * Cross-platform compatible (Linux, Windows).
 */

#include "mesh3d/MeshData.h"

#include <QWidget>
#include <Eigen/Core>
#include <memory>

// Forward declarations (avoid VTK headers in public interface)
class QVTKOpenGLNativeWidget;
class vtkRenderer;
class vtkActor;
class vtkPolyData;
class vtkTransform;
class vtkAxesActor;
class vtkOrientationMarkerWidget;

/// Camera view direction for fixed orthographic views
enum class CameraView {
    Free,   ///< Interactive free rotation (default)
    XY,     ///< Looking down Z axis (XY plane in screen)
    YZ,     ///< Looking along X axis (YZ plane in screen)
    XZ      ///< Looking along Y axis (XZ plane in screen)
};

/**
 * @brief Interactive 3D preview widget for STL orientation
 *
 * Features:
 * - Object rotation via mouse (trackball-style, but rotating object not camera)
 * - XY reference plane visualization
 * - Coordinate axes display
 * - Transformation matrix export for projection
 * - Fixed camera views for orthographic projections (YZ, XZ planes)
 *
 * Mouse controls:
 * - Left drag: Rotate object
 * - Middle drag: Pan (translate)
 * - Wheel: Zoom
 * - Right drag: Zoom
 */
class STLPreviewWidget : public QWidget {
    Q_OBJECT

public:
    explicit STLPreviewWidget(QWidget* parent = nullptr);
    ~STLPreviewWidget() override;

    /// Set the mesh to display
    void setMesh(std::shared_ptr<mesh3d::MeshData> mesh);

    /// Get current object transformation matrix
    Eigen::Matrix4d getObjectTransform() const;

    /// Set rotation angles directly (Euler angles in degrees: X, Y, Z)
    void setRotation(double rx, double ry, double rz);

    /// Get current rotation angles (Euler angles in degrees)
    void getRotation(double& rx, double& ry, double& rz) const;

    /// Reset transformation to identity
    void resetTransform();

    /// Center the mesh in view
    void centerMesh();

    // ── Camera view control ───────────────────────────────────────────────────

    /// Set fixed camera view direction
    void setCameraView(CameraView view);

    /// Get current camera view mode
    CameraView getCameraView() const { return m_cameraView; }

    /// Apply object transform from external source (for synchronization)
    void setObjectTransform(double rx, double ry, double rz);

    /// Enable/disable user interaction
    void setInteractive(bool interactive);

    /// Sync actor's mouse-driven rotation to our internal transform (called by callbacks)
    void syncActorRotation();

    // ── Quick alignment presets ──────────────────────────────────────────────

    void setViewTop();      ///< View from +Z (occlusal view, default)
    void setViewBottom();   ///< View from -Z
    void setViewFront();    ///< View from +Y
    void setViewBack();     ///< View from -Y
    void setViewLeft();     ///< View from -X
    void setViewRight();    ///< View from +X

    /// Rotate object 90° around specified axis
    void rotateX90();
    void rotateY90();
    void rotateZ90();

    /// Flip (180° rotation) around specified axis
    void flipX();
    void flipY();
    void flipZ();

signals:
    /// Emitted whenever the object transformation changes
    void transformChanged(const Eigen::Matrix4d& transform);

    /// Emitted when user starts interacting (mouse press)
    void interactionStarted();

    /// Emitted when user stops interacting (mouse release)
    void interactionEnded();

protected:
    void resizeEvent(QResizeEvent* event) override;

private:
    void setupVTK();
    void setupReferenceGrid();
    void setupAxesWidget();
    void updateMeshActor();
    void emitTransformChanged();
    void applyCameraView();

    // Convert CGAL mesh to VTK polydata
    void cgalToVTK(const mesh3d::SurfaceMesh& mesh, vtkPolyData* polyData);

    // VTK components (using raw pointers managed by VTK smart pointers internally)
    QVTKOpenGLNativeWidget* m_vtkWidget = nullptr;
    vtkRenderer* m_renderer = nullptr;
    vtkActor* m_meshActor = nullptr;
    vtkActor* m_gridActor = nullptr;
    vtkTransform* m_objectTransform = nullptr;
    vtkOrientationMarkerWidget* m_axesWidget = nullptr;

    // Data
    std::shared_ptr<mesh3d::MeshData> m_mesh;

    // Current Euler angles (degrees)
    double m_rotX = 0.0;
    double m_rotY = 0.0;
    double m_rotZ = 0.0;

    // Camera view mode
    CameraView m_cameraView = CameraView::Free;
};
