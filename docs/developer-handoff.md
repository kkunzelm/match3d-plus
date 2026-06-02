# Match3D+ – Developer Handoff

## Goal

Compare 2.5D dental surface scans (heightmaps) by aligning them and computing height differences for quantitative wear analysis. Originally developed for laser scanner data (VIFF format), now extended to support STL files from intraoral scanners via interactive 3D orientation and projection to 2D heightmaps.

Primary workflow:
1. Load two surface scans (Model = reference, Data = to be aligned)
2. Define matching ROI in both images
3. Coarse alignment (center-of-mass or landmarks)
4. Fine registration (4-DOF Align or 6-DOF Refine ICP)
5. Compute difference image
6. Analyze statistics and volume

---

## Build Environment (Debian 13 / Linux 6.x)

| Library | Version | Notes |
|---------|---------|-------|
| Qt | **6.4+** | Widgets, Concurrent modules |
| CMake | 3.20+ | C++20 required |
| CCCoreLib | submodule | CloudCompare point cloud library (LGPL/MIT) |
| VTK | 9.3 | Only for STL import feature |
| CGAL | 6.0+ | Only for STL import feature (header-only mode) |
| Eigen | 3.4+ | Only for STL import feature |

### CMake Configuration

```bash
# Standard build (with STL import)
mkdir build && cd build
cmake ..
make -j$(nproc)

# Without STL import (no VTK/CGAL/Eigen required)
cmake -DMATCH3D_ENABLE_STL_IMPORT=OFF ..

# Custom VTK path
cmake -DVTK_DIR=/path/to/vtk/lib/cmake/vtk-9.3 ..
```

### Windows Build Notes

The project supports Windows 11 / MSVC 2019+. Key CMake settings:
```cmake
if(WIN32)
    add_definitions(-DNOMINMAX)  # Prevent min/max macro conflicts
    if(MSVC)
        add_compile_options(/utf-8)  # UTF-8 source encoding
        add_compile_options(/wd4251 /wd4275)  # Suppress DLL export warnings
    endif()
endif()
```

---

## Source Layout

```
src/
├── main.cpp
├── MainWindow.{h,cpp}         Qt MDI main window, menus, image list
├── ImageWindow.{h,cpp}        MDI child window for each depth image
├── DepthImageView.{h,cpp}     Custom QWidget for 2D depth rendering
├── RoiMask.{h,cpp}            ROI polygon/rectangle/ellipse/strip logic
├── ImageProcessor.{h,cpp}     Z-range, gradient filters, histogram
├── AppSettings.{h,cpp}        Persistent QSettings wrapper
│
├── io/
│   ├── ViffReader.{h,cpp}     VIFF/XV file format reader
│   ├── ViffWriter.{h,cpp}     VIFF/XV file format writer
│   └── PlyIO.{h,cpp}          PLY export for 3D software
│
├── registration/
│   ├── Transformation3D.h     6-DOF transform struct (rotation + translation)
│   ├── CoarseRegistration.{h,cpp}   Center-of-mass, landmark-based alignment
│   ├── RegistrationWorker.{h,cpp}   Async ICP (4-DOF, 6-DOF, CCCoreLib)
│   ├── DifferenceCalculator.{h,cpp} Subtract aligned images
│   └── SVD3x3.{h,cpp}         3×3 SVD for rotation fitting
│
├── dialogs/
│   ├── GlobalParametersDialog.{h,cpp}  App-wide settings
│   ├── HistogramDialog.{h,cpp}         Interactive histogram + Z clipping
│   ├── MatchingControlPanel.{h,cpp}    Model/Data selection, registration controls
│   └── STLImportDialog.{h,cpp}         [STL feature] 3D preview + projection
│
├── mesh3d/                              [STL feature]
│   ├── MeshData.h               CGAL SurfaceMesh wrapper
│   ├── STLReader.{h,cpp}        Binary STL → CGAL mesh (winding-corrected)
│   └── MeshProjection.{h,cpp}   3D mesh → 2D heightmap (Z-buffer rasterization)
│
├── visualization3d/                     [STL feature]
│   └── STLPreviewWidget.{h,cpp}  VTK-based 3D preview with object rotation
│
└── tools/
    ├── SyntheticTestData.cpp    Generate test heightmaps for registration validation
    └── GenerateWearSamples.cpp  Generate wear samples (flat+depression, dome+facet)

extern/
├── CCCoreLib/     CloudCompare point cloud library (git submodule)
├── happly/        PLY I/O header-only library (MIT)
└── nanoflann/     KD-tree header-only library (BSD)
```

---

## Core Data Structures

### ViffImage (2.5D Heightmap)

```cpp
// src/io/ViffReader.h
struct ViffImage {
    uint32_t rows;         // Height (confusingly named in VIFF spec)
    uint32_t cols;         // Width
    float xPixelSize;      // Metres per pixel
    float yPixelSize;
    float originX, originY;
    std::vector<float> data;  // Row-major: data[row * cols + col]
    bool isDiffImage;         // true = allow negative/zero as valid

    float at(uint32_t row, uint32_t col) const;
    bool isValid(uint32_t row, uint32_t col) const;  // false if v==0 && !isDiffImage
};
```

**Important:** VIFF header stores dimensions in confusingly-named fields. The ViffReader corrects this:
- `header.numberOfRows` → `ViffImage.cols` (width)
- `header.numberOfColumns` → `ViffImage.rows` (height)

### Transformation3D

```cpp
// src/registration/Transformation3D.h
struct Transformation3D {
    double rotation[3][3];   // 3×3 rotation matrix
    double translation[3];   // Translation vector (tx, ty, tz)

    void apply(double& x, double& y, double& z) const;
    Transformation3D compose(const Transformation3D& other) const;
    Transformation3D inverse() const;
};
```

### RoiMask

Stores ROI selection as:
- Polygon vertices (screen coordinates)
- Bitmask for fast per-pixel lookup
- Support for: Polygon, Rectangle, Ellipse, Strips

---

## Registration Pipeline

```
┌─────────────┐     ┌─────────────┐
│   Model     │     │    Data     │
│ (Reference) │     │  (Moving)   │
└──────┬──────┘     └──────┬──────┘
       │                   │
       │   ┌───────────────┴───────────────┐
       │   │     Coarse Registration       │
       │   │  (from COM or from Points)    │
       │   └───────────────┬───────────────┘
       │                   │
       │   ┌───────────────┴───────────────┐
       │   │      Fine Registration        │
       │   │  Align (4-DOF) or Refine(6-DOF)│
       │   └───────────────┬───────────────┘
       │                   │
       └───────────┬───────┘
                   │
           ┌───────┴───────┐
           │  Diff Image   │
           │ (Model - Data)│
           └───────────────┘
```

### Coarse Registration

1. **from COM**: Center-of-mass alignment
   - Compute COM of both ROIs
   - Translate Data to match Model COM
   - Compute best-fit rotation (XY-plane only, around Z-axis)

2. **from Points**: Landmark-based
   - User clicks 3+ corresponding points in each image
   - SVD-based rigid transform fitting

### Fine Registration (RegistrationWorker)

Runs in separate thread via `QThread` + `moveToThread()`:

- **Align (4-DOF)**: Rotation around Z + XY translation + Z translation
  - Custom implementation optimized for 2.5D data
  - `RegistrationWorker::run4DOF()`

- **Refine (6-DOF)**: Full rigid transform
  - Point-to-plane ICP (Neugebauer algorithm)
  - `RegistrationWorker::run6DOF()`

- **CCCoreLib ICP**: Alternative using CloudCompare's ICP
  - `RegistrationWorker::runCCLibICP()`

### Difference Calculation

`DifferenceCalculator::compute()`:
1. Apply final transform to Data image
2. Bilinear interpolation at Model pixel locations
3. Compute height difference: `Model[x,y] - TransformedData[x,y]`
4. Result is a new ViffImage with `isDiffImage = true`

---

## STL Import Feature (Phase 1)

### Workflow

```
STL File → STLImportDialog → Interactive 3D Orientation → Z-Projection → ViffImage
```

### Key Components

1. **STLReader** (`mesh3d/STLReader.cpp`)
   - Reads binary STL files
   - Converts to CGAL SurfaceMesh
   - Includes winding correction (Primescan compatibility)

2. **STLPreviewWidget** (`visualization3d/STLPreviewWidget.cpp`)
   - VTK-based 3D preview with Phong shading
   - Object rotation (not camera!) via vtkInteractorStyleTrackballActor
   - XY reference grid for orientation
   - Axes widget (RGB = XYZ)

3. **MeshProjection** (`mesh3d/MeshProjection.cpp`)
   - Projects transformed mesh to 2D heightmap
   - Z-buffer algorithm (keeps maximum Z per pixel)
   - Barycentric triangle rasterization

4. **STLImportDialog** (`dialogs/STLImportDialog.cpp`)
   - 3D preview (left) + 2D projection preview (right)
   - 2D preview supports Graycast shading (Sobel-based, toggleable)
   - Quick alignment buttons (Top, Bottom, 90X, 90Y, 90Z, Reset)
   - Fine rotation sliders (X, Y, Z)
   - Resolution setting with global default from `AppSettings::stlResolution`
   - "Copy from" combo to match resolution of open images

### Resolution Management

For comparing scans from different scanners, consistent resolution is critical:

```cpp
// STLImportDialog constructor accepts settings and open images
STLImportDialog(const QString& filePath,
                AppSettings* settings,              // Global resolution default
                const std::vector<OpenImageInfo>& openImages,  // For "copy from"
                QWidget* parent);

// OpenImageInfo struct
struct OpenImageInfo {
    QString name;
    float xPixelSize;  // metres per pixel
    float yPixelSize;
};
```

The used resolution is saved back to `AppSettings::stlResolution` after import.

### Conditional Compilation

```cpp
#ifdef MATCH3D_STL_IMPORT_ENABLED
    // STL import code
#endif
```

Set via CMake option `MATCH3D_ENABLE_STL_IMPORT` (default: ON).

---

## Display Styles

`DepthImageView` supports multiple rendering modes:

| Style | Description |
|-------|-------------|
| **Linear** | Grayscale: min=black, max=white |
| **Linear2** | Grayscale: mean±3σ scaling (better contrast) |
| **FalseColor** | Blue-cyan-green-yellow-red colormap |
| **Graycast** | Shaded relief (gradient-based illumination) |

---

## Known Pitfalls / Historical Bugs

### VIFF Header Field Names

The VIFF format uses confusing field names:
- `numberOfRows` actually stores **width** (columns)
- `numberOfColumns` actually stores **height** (rows)

`ViffReader` and `ViffWriter` handle this internally.

### ViffImage Member Names

ViffImage uses `rows` and `cols`, not `width` and `height`:
```cpp
// CORRECT
img.cols = width;
img.rows = height;

// WRONG - these don't exist
img.width = ...;  // Compile error
```

### Qt6 Signal/Slot Syntax

Use new-style connects with method pointers:
```cpp
// CORRECT (Qt6)
connect(btn, &QPushButton::clicked, this, &MyClass::onClicked);

// WRONG (Qt5 style with SLOT() macro - may fail silently)
connect(btn, SIGNAL(clicked()), this, SLOT(onClicked()));
```

### Eigen3 CMake Target

Use `CONFIG` mode to find Eigen properly:
```cmake
# CORRECT
find_package(Eigen3 3.4 REQUIRED CONFIG)

# MAY FAIL
find_package(Eigen3 3.4 REQUIRED)  # Without CONFIG
```

### VTK Module Auto-Init

VTK requires factory registration:
```cmake
vtk_module_autoinit(TARGETS myapp MODULES ${VTK_LIBRARIES})
```

Without this, VTK rendering may fail silently.

### Non-Modal Dialogs

For dialogs that should allow continued interaction with other windows:
```cpp
// CORRECT - Non-modal dialog
auto* dlg = new QDialog(this);
dlg->setAttribute(Qt::WA_DeleteOnClose);
// ... setup dialog ...
dlg->show();  // Returns immediately

// WRONG - Modal dialog blocks all interaction
QDialog dlg(this);
dlg.exec();  // Blocks until closed
```

Used for: Statistics, Histogram, and Diff Image statistics dialogs.

### QString::arg Format Placeholders

Qt uses `%1`, `%2`, `%3` placeholders, not C-style `%.2f`:
```cpp
// CORRECT (Qt)
QString s = tr("Size: %1 × %2 × %3 mm")
    .arg(x, 0, 'f', 2)
    .arg(y, 0, 'f', 2)
    .arg(z, 0, 'f', 2);

// WRONG (C-style printf format - causes "Argument missing" warnings)
QString s = tr("Size: %.2f × %.2f × %.2f mm")
    .arg(x, 0, 'f', 2)  // These won't match %.2f
```

### imageWindows_ Contains Nullptr Entries

`MainWindow::imageWindows_` is a sparse vector — closed windows become `nullptr`:
```cpp
// WRONG - crashes on closed windows
for (const auto* w : imageWindows_) {
    w->someMethod();  // Crash if w is nullptr
}

// CORRECT - skip nullptr entries
for (const auto* w : imageWindows_) {
    if (!w) continue;
    w->someMethod();
}
```

The `imageWindows()` helper method returns only non-null entries.

---

## GUI Layout

### MainWindow

```
┌─────────────────────────────────────────────────────────────────┐
│  File  Edit  View  Filter  Register  Tools  Window  Help        │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  ┌─────────────────┐  ┌─────────────────────────────────────┐   │
│  │  Image List     │  │                                     │   │
│  │  ├── scan1.xv   │  │         MDI Area                    │   │
│  │  ├── scan2.xv   │  │    (ImageWindow instances)          │   │
│  │  └── diff.xv    │  │                                     │   │
│  │                 │  │                                     │   │
│  │  Matching Panel │  │                                     │   │
│  │  Model: [▼]     │  │                                     │   │
│  │  Data:  [▼]     │  │                                     │   │
│  │  [from COM]     │  │                                     │   │
│  │  [from Points]  │  │                                     │   │
│  │  [Align]        │  │                                     │   │
│  │  [Refine]       │  │                                     │   │
│  │  [Diff Image]   │  │                                     │   │
│  └─────────────────┘  └─────────────────────────────────────┘   │
│                                                                 │
│  Status: Ready                                                  │
└─────────────────────────────────────────────────────────────────┘
```

### ImageWindow (MDI Child)

Each depth image opens in its own `ImageWindow`:
- `DepthImageView` widget for rendering
- Context menu for ROI tools, display style
- Toolbar for common actions
- Status bar showing cursor position and Z value

---

## File Formats

### VIFF/XV (Native)

- 1024-byte header + float32 data
- Row-major pixel order
- Pixel size in metres (not mm!)
- See `docs/viff-format.md` for full specification

### STL (Import Only)

- Binary STL supported (80-byte header + triangles)
- Converted to ViffImage via Z-projection
- Original 3D data not preserved after import

### PLY (Export)

- Export heightmaps as 3D point clouds
- For use in CloudCompare, MeshLab, etc.

---

## Utility Tools

### synthetic_test_data

Generates synthetic heightmaps for testing registration:
```bash
./src/synthetic_test_data
# Creates: synthetic_model.xv, synthetic_data.xv
```

### generate_wear_samples

Generates wear samples for testing surface fitting:
```bash
./src/generate_wear_samples --help
# Options: flat_with_depression, dome_with_facet
```

---

## What Remains / Potential Improvements

- **Phase 2 STL Import**: Full 3D workflow (registration in 3D space, not via 2D projection)
- **Auto-orientation**: PCA-based initial alignment for STL files
- **Batch import**: Process multiple STL files with same orientation
- **Scanner presets**: Store per-scanner transformation defaults
- **Statistics export**: CSV export of volume/area measurements

---

## Changelog (Recent)

### 2026-06-02 – STL Import Feature

- Added STL file import with interactive 3D orientation
- VTK-based preview widget with object rotation
- Z-buffer projection to 2D heightmap
- Quick alignment buttons (Top, Bottom, 90X, etc.)
- Conditional compilation via `MATCH3D_ENABLE_STL_IMPORT`
- Graycast shading in 2D projection preview (toggleable)
- Resolution management: global default + "Copy from" open images
- Fixed quick alignment button i18n comparison
- Fixed QString format strings (use `%1` not `%.2f`)
- Fixed null pointer crash when iterating `imageWindows_`
- Winding order correction for Primescan STL files (per-face normal check)

### 2026-06-02 – Non-Modal Dialogs

- Statistics dialog now non-modal (multiple windows allowed)
- Histogram dialog now non-modal
- Diff Image workflow: opens image first, then shows non-modal statistics
- Pattern: use `show()` instead of `exec()`, set `Qt::WA_DeleteOnClose`

### 2026-06-01 – Linear2 Display Style

- Added mean±3σ scaling for better contrast in difference images
- Fixed ROI rendering in Graycast style

### Earlier – Surface Fitting

- Fit Plane: Least-squares plane fitting for flat samples
- Fit Sphere: Sphere fitting for ball specimens
- Volume statistics for wear measurement
- Wear sample generator tool

### Earlier – Registration Improvements

- 6-DOF point-to-plane ICP (Neugebauer)
- CCCoreLib ICP integration
- Improved coarse registration

---

## Quick Reference

### Adding a New Display Style

1. Add enum value in `DepthImageView.h`
2. Implement rendering in `DepthImageView::paintEvent()`
3. Add menu action in `ImageWindow::createMenus()`

### Adding a New Filter

1. Add method in `ImageProcessor`
2. Create menu action in `MainWindow` or `ImageWindow`
3. Apply filter and create new `ImageWindow` with result

### Adding a New Registration Method

1. Implement in `RegistrationWorker` or separate class
2. Add UI in `MatchingControlPanel`
3. Wire signals/slots in `MainWindow`

---

## Author

**Prof. Dr. Karl-Heinz Kunzelmann**

Qt6/C++20 re-implementation of Match3D 2.5 (originally by Wolfram Gloger, LMU Munich).

---

## License

GNU General Public License v2 or later. See LICENSE file.
