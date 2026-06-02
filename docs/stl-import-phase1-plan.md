# Match3D+ STL-Import – Phase 1 Implementierungsplan

**Datum:** 2026-06-02
**Ziel:** STL-Dateien importieren, interaktiv orientieren, auf 2.5D projizieren

---

## 1. Konzept

### 1.1 Workflow

```
┌──────────────────────────────────────────────────────────────────────┐
│                         STL IMPORT WORKFLOW                          │
├──────────────────────────────────────────────────────────────────────┤
│                                                                      │
│  1. File → Open STL...                                               │
│          ↓                                                           │
│  2. STLImportDialog öffnet sich                                      │
│          ↓                                                           │
│  3. Benutzer sieht 3D-Vorschau + XY-Referenzebene                   │
│          ↓                                                           │
│  4. Benutzer DREHT DAS OBJEKT (nicht die Kamera!)                   │
│     bis die Okklusalfläche nach oben zeigt                          │
│          ↓                                                           │
│  5. Live-2D-Vorschau zeigt Projektion auf XY-Ebene                  │
│          ↓                                                           │
│  6. "Importieren" → Transformation wird angewendet                   │
│          ↓                                                           │
│  7. Projiziertes Heightmap öffnet als normales ImageWindow          │
│          ↓                                                           │
│  8. Weiter mit bewährtem Match3D+ Workflow                          │
│                                                                      │
└──────────────────────────────────────────────────────────────────────┘
```

### 1.2 Interaktive Orientierung – Technisches Konzept

**Schlüsselidee:** Der Benutzer dreht das **Objekt**, nicht die Kamera.

```
        Feste Kamera (Draufsicht)
              │
              ▼
    ┌─────────────────────┐
    │    Z ▲              │
    │      │   Objekt     │
    │      │  ┌─────┐     │
    │      │  │ ◠◡◠ │ ←── Zahn wird gedreht
    │      │  └─────┘     │
    │      └────────► X   │
    │     /               │
    │    Y                │
    │ ═══════════════════ │ ←── XY-Referenzebene (grau, semi-transparent)
    └─────────────────────┘
```

**VTK-Implementierung:**

1. **Objekt-Transformation statt Kamera-Rotation:**
   ```cpp
   // vtkActor hat eine interne Transformationsmatrix
   vtkSmartPointer<vtkTransform> m_objectTransform = vtkSmartPointer<vtkTransform>::New();
   m_meshActor->SetUserTransform(m_objectTransform);
   ```

2. **Trackball auf Objekt anwenden:**
   ```cpp
   // Custom Interactor: Mausbewegung → Objekt-Rotation
   // Nicht vtkInteractorStyleTrackballCamera, sondern vtkInteractorStyleTrackballActor
   // Oder: Custom Implementation die m_objectTransform direkt manipuliert
   ```

3. **Transformation auslesen:**
   ```cpp
   vtkMatrix4x4* matrix = m_objectTransform->GetMatrix();
   // Diese Matrix wird auf das Mesh angewendet vor der Projektion
   ```

### 1.3 Dialog-Layout

```
┌─────────────────────────────────────────────────────────────────────────┐
│  STL Import: zahnbogen_scan.stl                                    [X] │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│  ┌─────────────────────────────────┐  ┌──────────────────────────────┐ │
│  │                                 │  │                              │ │
│  │                                 │  │      2D-Projektion           │ │
│  │      3D-Ansicht                 │  │      (Live-Vorschau)         │ │
│  │                                 │  │                              │ │
│  │   Linke Maus: Objekt drehen     │  │   ┌────────────────────┐     │ │
│  │   Mittlere Maus: Zoom           │  │   │   ▓▓▓▓▓▓▓▓▓▓▓     │     │ │
│  │   Rechte Maus: Verschieben      │  │   │  ▓▓▓▓▓▓▓▓▓▓▓▓▓    │     │ │
│  │                                 │  │   │   ▓▓▓▓▓▓▓▓▓▓▓     │     │ │
│  │   ═══════════════════           │  │   └────────────────────┘     │ │
│  │   (XY-Referenzebene)            │  │                              │ │
│  │                                 │  │                              │ │
│  └─────────────────────────────────┘  └──────────────────────────────┘ │
│                                                                         │
│  ┌─ Schnellausrichtung ──────────────────────────────────────────────┐ │
│  │  [ ↑ Draufsicht ]  [ ↓ Untersicht ]  [ ← Links ]  [ → Rechts ]    │ │
│  │  [ 90° X ]  [ 90° Y ]  [ 90° Z ]  [ Reset ]                       │ │
│  └───────────────────────────────────────────────────────────────────┘ │
│                                                                         │
│  ┌─ Feineinstellung ─────────────────────────────────────────────────┐ │
│  │  Rotation X: [────●────] -180° ... +180°    Wert: [ 45.0 ]°       │ │
│  │  Rotation Y: [────●────] -180° ... +180°    Wert: [  0.0 ]°       │ │
│  │  Rotation Z: [────●────] -180° ... +180°    Wert: [ 12.5 ]°       │ │
│  └───────────────────────────────────────────────────────────────────┘ │
│                                                                         │
│  ┌─ Projektion ──────────────────────────────────────────────────────┐ │
│  │  Auflösung: [ 0.025 ] mm/Pixel     Größe: 512 × 480 Pixel         │ │
│  │  [ ] Automatisch (aus Mesh-Größe)                                 │ │
│  └───────────────────────────────────────────────────────────────────┘ │
│                                                                         │
│  Mesh-Info: 125.432 Dreiecke, Bounds: 12.5 × 8.3 × 4.2 mm              │
│                                                                         │
│         [ Abbrechen ]                              [ Importieren ]      │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## 2. Komponenten

### 2.1 Neue Dateien

```
src/
├── mesh3d/                          (NEU)
│   ├── STLReader.h                  ← aus DentScanCompare
│   ├── STLReader.cpp                ← aus DentScanCompare
│   ├── MeshData.h                   ← vereinfachte ScanData-Struktur
│   └── MeshProjection.h/.cpp        ← Projektion auf reguläres Grid
│
├── dialogs/
│   └── STLImportDialog.h/.cpp       (NEU)
│
└── visualization3d/                 (NEU)
    └── STLPreviewWidget.h/.cpp      ← VTK-basierte 3D-Vorschau
```

### 2.2 Abhängigkeiten

| Bibliothek | Benötigt für | Bereits vorhanden |
|------------|--------------|-------------------|
| VTK 9.3 | 3D-Vorschau, Interaktion | ~/VTK-install-linux |
| CGAL | STL → SurfaceMesh | System (6.0.1) |
| Eigen | Transformationsmatrizen | System (3.4.0) |

**Nicht benötigt in Phase 1:**
- nanoflann (kein ICP)
- CurvatureAnalysis, ToothSegmentation (keine 3D-ROI)
- DistanceField (Differenz über 2D-Workflow)

---

## 3. Detaillierte Spezifikationen

### 3.1 MeshData (vereinfacht)

```cpp
// src/mesh3d/MeshData.h
#pragma once

#include <CGAL/Exact_predicates_inexact_constructions_kernel.h>
#include <CGAL/Surface_mesh.h>
#include <Eigen/Core>
#include <string>
#include <array>

namespace mesh3d {

using Kernel      = CGAL::Exact_predicates_inexact_constructions_kernel;
using Point3      = Kernel::Point_3;
using SurfaceMesh = CGAL::Surface_mesh<Point3>;

struct MeshData {
    std::string filePath;
    std::string stlHeader;        // 80-byte STL header
    SurfaceMesh mesh;

    // Bounds (nach Import, vor Transformation)
    std::array<double, 3> boundsMin{};
    std::array<double, 3> boundsMax{};
    std::size_t triangleCount = 0;

    // Aktuelle Transformation (wird interaktiv angepasst)
    Eigen::Matrix4d transform = Eigen::Matrix4d::Identity();
};

} // namespace mesh3d
```

### 3.2 STLReader

Direkt aus DentScanCompare übernehmen, minimale Anpassung:

```cpp
// src/mesh3d/STLReader.h
#pragma once

#include "MeshData.h"
#include <memory>
#include <string>

namespace mesh3d {

// Liest Binary STL, gibt MeshData zurück
// Winding-Korrektur (Primescan-Problem) ist integriert
std::shared_ptr<MeshData> readSTL(const std::string& filePath, std::string& errorMsg);

} // namespace mesh3d
```

### 3.3 STLPreviewWidget

```cpp
// src/visualization3d/STLPreviewWidget.h
#pragma once

#include "mesh3d/MeshData.h"
#include <QWidget>
#include <vtkSmartPointer.h>
#include <Eigen/Core>

class QVTKOpenGLNativeWidget;
class vtkActor;
class vtkTransform;
class vtkRenderer;

class STLPreviewWidget : public QWidget {
    Q_OBJECT
public:
    explicit STLPreviewWidget(QWidget* parent = nullptr);

    // Mesh setzen (erzeugt vtkPolyData)
    void setMesh(std::shared_ptr<mesh3d::MeshData> mesh);

    // Transformation
    void setRotation(double rx, double ry, double rz);  // Euler-Winkel in Grad
    void resetTransform();

    // Schnellausrichtung
    void setViewTop();      // Draufsicht (Standard)
    void setViewBottom();   // Untersicht
    void setViewLeft();
    void setViewRight();
    void rotateX90();
    void rotateY90();
    void rotateZ90();

    // Aktuelle Transformation auslesen (für Projektion)
    Eigen::Matrix4d getObjectTransform() const;

signals:
    // Wird bei jeder Änderung der Transformation emittiert
    void transformChanged(const Eigen::Matrix4d& transform);

private:
    void setupVTK();
    void setupInteraction();  // Objekt-Trackball, nicht Kamera
    void updateReferenceGrid();

    QVTKOpenGLNativeWidget* m_vtkWidget = nullptr;
    vtkSmartPointer<vtkRenderer> m_renderer;
    vtkSmartPointer<vtkActor> m_meshActor;
    vtkSmartPointer<vtkActor> m_gridActor;  // XY-Referenzebene
    vtkSmartPointer<vtkTransform> m_objectTransform;

    std::shared_ptr<mesh3d::MeshData> m_mesh;
};
```

### 3.4 MeshProjection

```cpp
// src/mesh3d/MeshProjection.h
#pragma once

#include "MeshData.h"
#include "io/ViffReader.h"  // Für ViffImage-Struktur
#include <Eigen/Core>

namespace mesh3d {

struct ProjectionParams {
    double resolution = 0.025;  // mm pro Pixel
    bool autoSize = true;       // Größe aus Mesh-Bounds
    int width = 512;            // Falls !autoSize
    int height = 512;
    double noDataValue = 0.0;   // Für Pixel ohne Mesh-Treffer
};

// Projiziert transformiertes Mesh auf XY-Ebene
// Gibt ViffImage zurück (kompatibel mit bestehendem Match3D+ Code)
ViffImage projectToHeightmap(
    const MeshData& mesh,
    const Eigen::Matrix4d& transform,
    const ProjectionParams& params = {}
);

// Hilfsfunktion: Berechnet optimale Bounds nach Transformation
void computeTransformedBounds(
    const MeshData& mesh,
    const Eigen::Matrix4d& transform,
    std::array<double, 3>& outMin,
    std::array<double, 3>& outMax
);

} // namespace mesh3d
```

### 3.5 STLImportDialog

```cpp
// src/dialogs/STLImportDialog.h
#pragma once

#include "mesh3d/MeshData.h"
#include "io/ViffReader.h"
#include <QDialog>
#include <memory>

class STLPreviewWidget;
class QLabel;
class QSlider;
class QDoubleSpinBox;

class STLImportDialog : public QDialog {
    Q_OBJECT
public:
    explicit STLImportDialog(const QString& filePath, QWidget* parent = nullptr);

    // Ergebnis nach accept()
    ViffImage getProjectedImage() const { return m_result; }
    QString getSourcePath() const { return m_filePath; }

private slots:
    void onTransformChanged();
    void onQuickAlign(int preset);  // 0=Top, 1=Bottom, 2=Left, ...
    void onRotationSliderChanged();
    void onResolutionChanged();
    void updateProjectionPreview();

private:
    void setupUI();
    void loadSTL();
    void updateMeshInfo();

    QString m_filePath;
    std::shared_ptr<mesh3d::MeshData> m_mesh;
    ViffImage m_result;

    // Widgets
    STLPreviewWidget* m_preview3D = nullptr;
    QLabel* m_preview2D = nullptr;  // QImage-basierte 2D-Vorschau
    QSlider* m_rotX = nullptr;
    QSlider* m_rotY = nullptr;
    QSlider* m_rotZ = nullptr;
    QDoubleSpinBox* m_resolution = nullptr;
    QLabel* m_meshInfo = nullptr;
};
```

### 3.6 Projektions-Algorithmus

```cpp
// Konzept für MeshProjection.cpp

ViffImage projectToHeightmap(const MeshData& mesh,
                              const Eigen::Matrix4d& transform,
                              const ProjectionParams& params) {
    // 1. Transformierte Bounds berechnen
    std::array<double, 3> tMin, tMax;
    computeTransformedBounds(mesh, transform, tMin, tMax);

    // 2. Grid-Größe bestimmen
    int width, height;
    if (params.autoSize) {
        width  = static_cast<int>((tMax[0] - tMin[0]) / params.resolution) + 1;
        height = static_cast<int>((tMax[1] - tMin[1]) / params.resolution) + 1;
    } else {
        width = params.width;
        height = params.height;
    }

    // 3. Z-Buffer initialisieren (höchster Z-Wert pro Pixel)
    std::vector<float> zBuffer(width * height, -std::numeric_limits<float>::infinity());

    // 4. Für jedes Dreieck im Mesh:
    for (auto face : mesh.mesh.faces()) {
        // a) Vertices transformieren
        // b) Auf XY-Ebene projizieren (Z ignorieren für Position)
        // c) Rasterisieren (Scanline oder Barycentric)
        // d) Für jeden Pixel: Z-Wert in Buffer schreiben (Maximum)
    }

    // 5. ViffImage erzeugen
    ViffImage result;
    result.width = width;
    result.height = height;
    result.xPixelSize = params.resolution;
    result.yPixelSize = params.resolution;
    result.data.resize(width * height);

    for (int i = 0; i < width * height; ++i) {
        if (zBuffer[i] == -std::numeric_limits<float>::infinity()) {
            result.data[i] = params.noDataValue;  // Kein Treffer
        } else {
            result.data[i] = zBuffer[i];
        }
    }

    return result;
}
```

---

## 4. Implementierungsphasen

### Phase 1.1: Build-System (Tag 1)

**Aufgaben:**

1. CMakeLists.txt erweitern:
   ```cmake
   # Root CMakeLists.txt
   set(VTK_DIR "$ENV{HOME}/VTK-install-linux/lib/cmake/vtk-9.3")
   find_package(VTK 9.3 REQUIRED COMPONENTS
       CommonCore CommonDataModel FiltersSources FiltersGeneral
       InteractionStyle RenderingCore RenderingOpenGL2 GUISupportQt
   )
   find_package(CGAL 6.0 REQUIRED)
   find_package(Eigen3 3.4 REQUIRED)
   ```

2. src/CMakeLists.txt:
   ```cmake
   # Mesh3D Library
   add_library(mesh3d_core STATIC
       mesh3d/MeshData.h
       mesh3d/STLReader.h mesh3d/STLReader.cpp
       mesh3d/MeshProjection.h mesh3d/MeshProjection.cpp
   )
   target_link_libraries(mesh3d_core PUBLIC CGAL::CGAL Eigen3::Eigen)

   # Visualization
   add_library(viz3d STATIC
       visualization3d/STLPreviewWidget.h
       visualization3d/STLPreviewWidget.cpp
   )
   target_link_libraries(viz3d PUBLIC mesh3d_core Qt6::Widgets ${VTK_LIBRARIES})
   vtk_module_autoinit(TARGETS viz3d MODULES ${VTK_LIBRARIES})

   # Main App erweitern
   target_sources(match3d_plus PRIVATE
       dialogs/STLImportDialog.h dialogs/STLImportDialog.cpp
   )
   target_link_libraries(match3d_plus PRIVATE mesh3d_core viz3d)
   ```

3. Verzeichnisse anlegen und Build testen

**Verifizierung:**
- `cmake -B build && cmake --build build` kompiliert
- VTK-, CGAL-, Eigen-Header werden gefunden

---

### Phase 1.2: STL-Reader (Tag 1-2)

**Aufgaben:**

1. `STLReader.cpp` aus DentScanCompare kopieren und anpassen:
   - `ScanData` → `MeshData`
   - Namespace `STLReader` → `mesh3d`

2. `MeshData.h` erstellen (siehe Spezifikation oben)

3. Einfacher Test:
   ```cpp
   // In main.cpp oder separater Test
   std::string err;
   auto mesh = mesh3d::readSTL("test.stl", err);
   if (mesh) {
       qDebug() << "Loaded" << mesh->triangleCount << "triangles";
   }
   ```

**Verifizierung:**
- STL-Datei wird geladen
- Dreieckszahl und Bounds plausibel

---

### Phase 1.3: VTK-Vorschau mit Objekt-Rotation (Tag 2-3)

**Aufgaben:**

1. `STLPreviewWidget` implementieren:
   - CGAL Mesh → vtkPolyData Konvertierung
   - XY-Referenzebene als semi-transparentes Grid
   - Objekt-Trackball (vtkInteractorStyleTrackballActor oder custom)

2. Transformation tracken:
   ```cpp
   // Bei jeder Mausbewegung
   void STLPreviewWidget::onInteraction() {
       // Objekt-Transform aus vtkActor auslesen
       vtkMatrix4x4* vtk_mat = m_meshActor->GetUserMatrix();
       Eigen::Matrix4d eigen_mat;
       for (int i = 0; i < 4; ++i)
           for (int j = 0; j < 4; ++j)
               eigen_mat(i, j) = vtk_mat->GetElement(i, j);

       emit transformChanged(eigen_mat);
   }
   ```

3. Schnellausrichtungs-Buttons (90°-Schritte)

**Verifizierung:**
- Mesh erscheint in 3D-Ansicht
- Objekt lässt sich drehen (Kamera bleibt fest)
- Referenzebene bleibt horizontal

---

### Phase 1.4: Projektion (Tag 3-4)

**Aufgaben:**

1. `MeshProjection.cpp` implementieren:
   - Dreieck-Rasterisierung (Scanline oder Barycentric)
   - Z-Buffer für Maximum-Wert pro Pixel
   - ViffImage-Ausgabe

2. Performance-Optimierung:
   - Nur sichtbare Dreiecke (Backface Culling)
   - Bounding-Box-Test pro Dreieck

**Verifizierung:**
- Projektion erzeugt plausibles Heightmap
- Auflösung korrekt (mm/Pixel)

---

### Phase 1.5: Import-Dialog (Tag 4-5)

**Aufgaben:**

1. `STLImportDialog` Layout erstellen (siehe Mockup oben)

2. Komponenten verbinden:
   - 3D-Vorschau links
   - 2D-Projektion rechts (QLabel mit QImage)
   - Slider für Rotation
   - Resolution-Spinbox

3. Live-Update:
   ```cpp
   connect(m_preview3D, &STLPreviewWidget::transformChanged,
           this, &STLImportDialog::updateProjectionPreview);
   ```

4. "Importieren" Button:
   - Finale Projektion berechnen
   - ViffImage in m_result speichern
   - accept()

**Verifizierung:**
- Dialog öffnet sich mit 3D-Vorschau
- Rotation wird live in 2D-Vorschau reflektiert
- Import erzeugt ViffImage

---

### Phase 1.6: MainWindow-Integration (Tag 5)

**Aufgaben:**

1. Menü: File → Open STL...

2. Slot:
   ```cpp
   void MainWindow::openSTL() {
       QString path = QFileDialog::getOpenFileName(this,
           "Open STL", QString(), "STL Files (*.stl)");
       if (path.isEmpty()) return;

       STLImportDialog dlg(path, this);
       if (dlg.exec() == QDialog::Accepted) {
           ViffImage img = dlg.getProjectedImage();
           // Neues ImageWindow erstellen (wie bei VIFF)
           createImageWindow(img, QFileInfo(path).fileName() + " (projected)");
       }
   }
   ```

3. Ab hier: normaler Match3D+ Workflow

**Verifizierung:**
- File → Open STL öffnet Dialog
- Nach Import: ImageWindow mit Heightmap
- ROI, Filter, Registration funktionieren wie gewohnt

---

## 5. Zusammenfassung

| Phase | Inhalt | Aufwand |
|-------|--------|---------|
| 1.1 | CMake-Integration | 0.5 Tag |
| 1.2 | STL-Reader | 0.5 Tag |
| 1.3 | VTK-Vorschau + Objekt-Rotation | 1.5 Tage |
| 1.4 | Projektion auf Heightmap | 1 Tag |
| 1.5 | Import-Dialog | 1 Tag |
| 1.6 | MainWindow-Integration | 0.5 Tag |
| **Gesamt** | | **~5 Tage** |

---

## 6. Erweiterungsmöglichkeiten (später)

Nach Phase 1 können basierend auf Erfahrung hinzukommen:

- **Preset-Speicherung:** Scanner-spezifische Transformationen merken
- **Batch-Import:** Mehrere STL mit gleicher Transformation
- **Auto-Orientierung:** PCA-basierte Vorausrichtung als Startpunkt
- **Direkte Registrierung:** Zwei STL → zwei projizierte Heightmaps → Match

---

## 7. Nächste Schritte

1. **Bestätigung** des Plans
2. **Phase 1.1 starten:** CMakeLists.txt erweitern
3. **STLReader** aus DentScanCompare kopieren und anpassen

Soll ich mit Phase 1.1 (CMake) beginnen?
