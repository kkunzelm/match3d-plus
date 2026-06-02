#include "STLPreviewWidget.h"

#include <QVBoxLayout>
#include <QVTKOpenGLNativeWidget.h>

#include <vtkActor.h>
#include <vtkAxesActor.h>
#include <vtkCamera.h>
#include <vtkCallbackCommand.h>
#include <vtkCellArray.h>
#include <vtkGenericOpenGLRenderWindow.h>
#include <vtkInteractorStyleTrackballActor.h>
#include <vtkNew.h>
#include <vtkOrientationMarkerWidget.h>
#include <vtkPlaneSource.h>
#include <vtkPoints.h>
#include <vtkPolyData.h>
#include <vtkPolyDataMapper.h>
#include <vtkProperty.h>
#include <vtkRenderer.h>
#include <vtkRenderWindow.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkTransform.h>
#include <vtkTransformPolyDataFilter.h>

#include <cmath>

// Callback for interaction events
namespace {
    void onInteractionStart(vtkObject*, unsigned long, void* clientData, void*) {
        auto* widget = static_cast<STLPreviewWidget*>(clientData);
        emit widget->interactionStarted();
    }

    void onInteractionEnd(vtkObject*, unsigned long, void* clientData, void*) {
        auto* widget = static_cast<STLPreviewWidget*>(clientData);
        // Sync actor orientation to our transform before signaling end
        widget->syncActorRotation();
        emit widget->interactionEnded();
    }

    void onInteraction(vtkObject*, unsigned long, void* clientData, void*) {
        auto* widget = static_cast<STLPreviewWidget*>(clientData);
        // Sync rotation during interaction for real-time updates
        widget->syncActorRotation();
    }
}

// ── Constructor / Destructor ─────────────────────────────────────────────────

STLPreviewWidget::STLPreviewWidget(QWidget* parent)
    : QWidget(parent)
{
    setupVTK();
}

STLPreviewWidget::~STLPreviewWidget()
{
    // VTK handles cleanup via smart pointers
    if (m_axesWidget) {
        m_axesWidget->SetInteractor(nullptr);
        m_axesWidget->Delete();
    }
}

// ── Setup ────────────────────────────────────────────────────────────────────

void STLPreviewWidget::setupVTK()
{
    // Create layout
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    // Create VTK widget
    m_vtkWidget = new QVTKOpenGLNativeWidget(this);
    layout->addWidget(m_vtkWidget);

    // Create render window
    vtkNew<vtkGenericOpenGLRenderWindow> renderWindow;
    m_vtkWidget->setRenderWindow(renderWindow);

    // Create renderer
    vtkNew<vtkRenderer> renderer;
    m_renderer = renderer.Get();
    renderWindow->AddRenderer(m_renderer);

    // Set background color (dark gray gradient)
    m_renderer->SetBackground(0.2, 0.2, 0.25);
    m_renderer->SetBackground2(0.4, 0.4, 0.45);
    m_renderer->GradientBackgroundOn();

    // Create object transform
    vtkNew<vtkTransform> transform;
    m_objectTransform = transform.Get();
    m_objectTransform->Register(nullptr);  // Keep alive

    // Setup interaction style: trackball on ACTOR (object rotation)
    vtkNew<vtkInteractorStyleTrackballActor> style;
    m_vtkWidget->interactor()->SetInteractorStyle(style);

    // Setup interaction callbacks
    vtkNew<vtkCallbackCommand> startCallback;
    startCallback->SetCallback(onInteractionStart);
    startCallback->SetClientData(this);
    m_vtkWidget->interactor()->AddObserver(vtkCommand::StartInteractionEvent, startCallback);

    vtkNew<vtkCallbackCommand> endCallback;
    endCallback->SetCallback(onInteractionEnd);
    endCallback->SetClientData(this);
    m_vtkWidget->interactor()->AddObserver(vtkCommand::EndInteractionEvent, endCallback);

    // Observer for during-interaction updates (real-time sync)
    vtkNew<vtkCallbackCommand> interactionCallback;
    interactionCallback->SetCallback(onInteraction);
    interactionCallback->SetClientData(this);
    m_vtkWidget->interactor()->AddObserver(vtkCommand::InteractionEvent, interactionCallback);

    // Setup reference grid and axes
    setupReferenceGrid();
    setupAxesWidget();

    // Initial camera position (looking down Z axis)
    applyCameraView();
}

void STLPreviewWidget::setupReferenceGrid()
{
    // Create XY plane grid
    vtkNew<vtkPlaneSource> plane;
    plane->SetOrigin(-50, -50, 0);
    plane->SetPoint1(50, -50, 0);
    plane->SetPoint2(-50, 50, 0);
    plane->SetXResolution(20);
    plane->SetYResolution(20);

    vtkNew<vtkPolyDataMapper> mapper;
    mapper->SetInputConnection(plane->GetOutputPort());

    vtkNew<vtkActor> actor;
    m_gridActor = actor.Get();
    m_gridActor->SetMapper(mapper);
    m_gridActor->GetProperty()->SetRepresentationToWireframe();
    m_gridActor->GetProperty()->SetColor(0.5, 0.5, 0.5);
    m_gridActor->GetProperty()->SetOpacity(0.3);
    m_gridActor->GetProperty()->SetLineWidth(1.0);
    m_gridActor->PickableOff();  // Prevent interaction - grid stays fixed

    m_renderer->AddActor(m_gridActor);
}

void STLPreviewWidget::setupAxesWidget()
{
    vtkNew<vtkAxesActor> axes;
    axes->SetTotalLength(10, 10, 10);
    axes->SetShaftTypeToCylinder();

    m_axesWidget = vtkOrientationMarkerWidget::New();
    m_axesWidget->SetOrientationMarker(axes);
    m_axesWidget->SetInteractor(m_vtkWidget->interactor());
    m_axesWidget->SetViewport(0.0, 0.0, 0.2, 0.2);
    m_axesWidget->EnabledOn();
    m_axesWidget->InteractiveOff();
}

// ── Mesh Handling ────────────────────────────────────────────────────────────

void STLPreviewWidget::setMesh(std::shared_ptr<mesh3d::MeshData> mesh)
{
    m_mesh = mesh;

    if (!m_mesh || !m_mesh->isValid()) {
        if (m_meshActor) {
            m_renderer->RemoveActor(m_meshActor);
            m_meshActor = nullptr;
        }
        return;
    }

    updateMeshActor();
    centerMesh();
    m_vtkWidget->renderWindow()->Render();
}

void STLPreviewWidget::updateMeshActor()
{
    if (!m_mesh) return;

    // Convert CGAL mesh to VTK
    vtkNew<vtkPolyData> polyData;
    cgalToVTK(m_mesh->mesh, polyData);

    // Apply transform
    vtkNew<vtkTransformPolyDataFilter> transformFilter;
    transformFilter->SetInputData(polyData);
    transformFilter->SetTransform(m_objectTransform);

    vtkNew<vtkPolyDataMapper> mapper;
    mapper->SetInputConnection(transformFilter->GetOutputPort());

    // Create or update actor
    if (!m_meshActor) {
        vtkNew<vtkActor> actor;
        m_meshActor = actor.Get();
        m_meshActor->Register(nullptr);  // Keep alive
        m_renderer->AddActor(m_meshActor);
    }

    m_meshActor->SetMapper(mapper);
    m_meshActor->GetProperty()->SetColor(0.9, 0.9, 0.85);  // Tooth-like color
    m_meshActor->GetProperty()->SetSpecular(0.3);
    m_meshActor->GetProperty()->SetSpecularPower(20);
}

void STLPreviewWidget::cgalToVTK(const mesh3d::SurfaceMesh& mesh, vtkPolyData* polyData)
{
    // Create points
    vtkNew<vtkPoints> points;
    points->SetNumberOfPoints(static_cast<vtkIdType>(mesh.number_of_vertices()));

    // Map CGAL vertex indices to VTK point indices
    std::map<mesh3d::VertexDesc, vtkIdType> vertexMap;
    vtkIdType idx = 0;

    for (auto v : mesh.vertices()) {
        const mesh3d::Point3& p = mesh.point(v);
        points->SetPoint(idx, p.x(), p.y(), p.z());
        vertexMap[v] = idx;
        ++idx;
    }

    // Create triangles
    vtkNew<vtkCellArray> triangles;
    for (auto f : mesh.faces()) {
        auto h = mesh.halfedge(f);
        auto v0 = mesh.target(h);
        auto v1 = mesh.target(mesh.next(h));
        auto v2 = mesh.target(mesh.next(mesh.next(h)));

        vtkIdType tri[3] = {vertexMap[v0], vertexMap[v1], vertexMap[v2]};
        triangles->InsertNextCell(3, tri);
    }

    polyData->SetPoints(points);
    polyData->SetPolys(triangles);
}

// ── Transformation ───────────────────────────────────────────────────────────

Eigen::Matrix4d STLPreviewWidget::getObjectTransform() const
{
    Eigen::Matrix4d result = Eigen::Matrix4d::Identity();

    if (m_objectTransform) {
        vtkMatrix4x4* vtkMat = m_objectTransform->GetMatrix();
        for (int i = 0; i < 4; ++i) {
            for (int j = 0; j < 4; ++j) {
                result(i, j) = vtkMat->GetElement(i, j);
            }
        }
    }

    return result;
}

void STLPreviewWidget::setRotation(double rx, double ry, double rz)
{
    m_rotX = rx;
    m_rotY = ry;
    m_rotZ = rz;

    m_objectTransform->Identity();

    // Apply rotations in ZYX order (common convention)
    m_objectTransform->RotateZ(rz);
    m_objectTransform->RotateY(ry);
    m_objectTransform->RotateX(rx);

    updateMeshActor();
    emitTransformChanged();
    m_vtkWidget->renderWindow()->Render();
}

void STLPreviewWidget::getRotation(double& rx, double& ry, double& rz) const
{
    rx = m_rotX;
    ry = m_rotY;
    rz = m_rotZ;
}

void STLPreviewWidget::resetTransform()
{
    setRotation(0, 0, 0);
}

void STLPreviewWidget::centerMesh()
{
    if (!m_mesh) return;

    m_renderer->ResetCamera();
    m_vtkWidget->renderWindow()->Render();
}

// ── Quick Alignment Presets ──────────────────────────────────────────────────

void STLPreviewWidget::setViewTop()
{
    setRotation(0, 0, 0);
}

void STLPreviewWidget::setViewBottom()
{
    setRotation(180, 0, 0);
}

void STLPreviewWidget::setViewFront()
{
    setRotation(-90, 0, 0);
}

void STLPreviewWidget::setViewBack()
{
    setRotation(90, 0, 0);
}

void STLPreviewWidget::setViewLeft()
{
    setRotation(-90, 0, 90);
}

void STLPreviewWidget::setViewRight()
{
    setRotation(-90, 0, -90);
}

void STLPreviewWidget::rotateX90()
{
    setRotation(m_rotX + 90, m_rotY, m_rotZ);
}

void STLPreviewWidget::rotateY90()
{
    setRotation(m_rotX, m_rotY + 90, m_rotZ);
}

void STLPreviewWidget::rotateZ90()
{
    setRotation(m_rotX, m_rotY, m_rotZ + 90);
}

void STLPreviewWidget::flipX()
{
    setRotation(m_rotX + 180, m_rotY, m_rotZ);
}

void STLPreviewWidget::flipY()
{
    setRotation(m_rotX, m_rotY + 180, m_rotZ);
}

void STLPreviewWidget::flipZ()
{
    setRotation(m_rotX, m_rotY, m_rotZ + 180);
}

// ── Events ───────────────────────────────────────────────────────────────────

void STLPreviewWidget::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    if (m_vtkWidget) {
        m_vtkWidget->renderWindow()->Render();
    }
}

void STLPreviewWidget::emitTransformChanged()
{
    emit transformChanged(getObjectTransform());
}

// ── Camera View Control ──────────────────────────────────────────────────────

void STLPreviewWidget::setCameraView(CameraView view)
{
    m_cameraView = view;
    applyCameraView();
    if (m_mesh) {
        centerMesh();
    }
}

void STLPreviewWidget::applyCameraView()
{
    if (!m_renderer) return;

    vtkCamera* cam = m_renderer->GetActiveCamera();

    switch (m_cameraView) {
    case CameraView::Free:
    case CameraView::XY:
        // Looking down Z axis (XY plane in screen) - default top view
        cam->SetPosition(0, 0, 100);
        cam->SetFocalPoint(0, 0, 0);
        cam->SetViewUp(0, 1, 0);
        break;

    case CameraView::YZ:
        // Looking along X axis (YZ plane in screen) - side view
        cam->SetPosition(100, 0, 0);
        cam->SetFocalPoint(0, 0, 0);
        cam->SetViewUp(0, 0, 1);
        break;

    case CameraView::XZ:
        // Looking along Y axis (XZ plane in screen) - front view
        cam->SetPosition(0, -100, 0);
        cam->SetFocalPoint(0, 0, 0);
        cam->SetViewUp(0, 0, 1);
        break;
    }

    if (m_vtkWidget && m_vtkWidget->renderWindow()) {
        m_vtkWidget->renderWindow()->Render();
    }
}

void STLPreviewWidget::setObjectTransform(double rx, double ry, double rz)
{
    // Apply transform without emitting signal (to avoid feedback loops)
    m_rotX = rx;
    m_rotY = ry;
    m_rotZ = rz;

    m_objectTransform->Identity();
    m_objectTransform->RotateZ(rz);
    m_objectTransform->RotateY(ry);
    m_objectTransform->RotateX(rx);

    updateMeshActor();
    if (m_vtkWidget && m_vtkWidget->renderWindow()) {
        m_vtkWidget->renderWindow()->Render();
    }
}

void STLPreviewWidget::setInteractive(bool interactive)
{
    if (m_vtkWidget && m_vtkWidget->interactor()) {
        if (interactive) {
            m_vtkWidget->interactor()->EnableRenderOn();
        } else {
            // Disable interaction by using a null style or restricted style
            // For now, we still allow zoom/pan but keep the view direction
        }
    }
}

void STLPreviewWidget::syncActorRotation()
{
    if (!m_meshActor) return;

    // Get the actor's current orientation (set by trackball interaction)
    double* actorOrientation = m_meshActor->GetOrientation();  // Returns [rx, ry, rz] in degrees

    // Only sync if there's actual rotation from the actor
    if (std::abs(actorOrientation[0]) < 0.01 &&
        std::abs(actorOrientation[1]) < 0.01 &&
        std::abs(actorOrientation[2]) < 0.01) {
        return;  // No significant rotation to sync
    }

    // Combine actor rotation with our current rotation
    // The actor orientation is applied after our transform, so we need to compose them
    double newRotX = m_rotX + actorOrientation[0];
    double newRotY = m_rotY + actorOrientation[1];
    double newRotZ = m_rotZ + actorOrientation[2];

    // Normalize angles to [-180, 180]
    auto normalize = [](double angle) {
        while (angle > 180.0) angle -= 360.0;
        while (angle < -180.0) angle += 360.0;
        return angle;
    };
    newRotX = normalize(newRotX);
    newRotY = normalize(newRotY);
    newRotZ = normalize(newRotZ);

    // Reset the actor's orientation (we'll apply everything through m_objectTransform)
    m_meshActor->SetOrientation(0, 0, 0);
    m_meshActor->SetPosition(0, 0, 0);

    // Update our internal state
    m_rotX = newRotX;
    m_rotY = newRotY;
    m_rotZ = newRotZ;

    // Update m_objectTransform with the new combined rotation
    m_objectTransform->Identity();
    m_objectTransform->RotateZ(m_rotZ);
    m_objectTransform->RotateY(m_rotY);
    m_objectTransform->RotateX(m_rotX);

    // Update the mesh to reflect the new transform
    updateMeshActor();

    // Emit signal so other views can sync
    emitTransformChanged();
}
