# Match3D v2 Development History

This file documents the development history of Match3D v2, a Qt6/C++20 re-implementation of the original Match3D 2.5 dental surface comparison software.

## Project Overview

**Purpose:** Compare two 3D surface scans (heightmaps) by aligning them and computing height differences. Primary use case: dental wear studies.

**Technology Stack:**
- Qt6 / C++20
- CCCoreLib (from CloudCompare) for point cloud operations
- CMake build system

**Key Data Format:** VIFF (.xv) files containing 2.5D depth images (heightmaps)

---

## Commit History

### 727fbe2 - Initial commit
- Basic Qt6 application structure
- VIFF file reading
- Image window with basic visualization

### 0f01b30 - Add scroll bar, Graycast style, and landmark point-pair picking
- Horizontal scroll bar for viewing long datasets
- **Graycast rendering style**: Lambertian diffuse shading based on surface gradients
  - Uses Sobel kernels for gradient computation
  - `theta = atan(sqrt(gx² + gy²))`, `shading = cos(theta)`
  - Flat surfaces (cos==1) rendered black (scanner convention)
- Landmark point picking with Shift+Click for coarse registration

### 7bb037e - Add ROI editing, histogram, statistics, and gradient clipping
- ROI (Region of Interest) editing: polygon, rectangle, ellipse tools
- Histogram dialog with statistics
- Gradient clipping for display range control

### 5d4315c - Implement 2.5D-specific registration algorithm
- **Key insight:** Standard 3D ICP fails due to scale imbalance (Z >> X,Y in depth images)
- Custom 2.5D registration approach:
  - Finds correspondences based on (X,Y) grid position, not 3D distance
  - Rotation only around Z-axis (alpha)
  - Translation in all three axes (tx, ty, tz)
  - Z-offset computed as median (robust to outliers)
- Coarse registration methods:
  - **from COM**: Center of mass translation
  - **from Points**: Landmark-based 2D rotation + translation

### c590417 - Add registration algorithm documentation
- Technical LaTeX documentation with mathematical formulations
- User manual in Markdown

### f2f4574 - Add 6-DOF point-to-plane refinement (Neugebauer algorithm)
- **Align button (4-DOF)**: alpha + tx, ty, tz (original Match)
- **Refine button (6-DOF)**: Full Euler angles (alpha, beta, gamma) + translation
- Neugebauer's point-to-plane ICP:
  - Surface normals from gradients: n = (-gx, -gy, 1) / ||...||
  - Point-to-plane distance: d = n · (T(p_d) - p_m)
  - Linearization for small angles
  - 6x6 normal equation system, Cholesky solver
  - Outlier rejection at 2.58 × RMS (99% quantile)

### c1fcf7c - Update documentation for Align/Refine workflow
- Updated pipeline overview (4 stages: Coarse → Align → Refine → Diff)
- Corrected color scheme: Red (negative) → Black (zero) → White (positive)
- Added 6-DOF section to technical docs with full mathematical derivation

### 567723b - Enable multi-file selection in Open dialogs
- Changed `getOpenFileName` to `getOpenFileNames`
- Supports Ctrl+Click (individual) and Shift+Click (range) selection
- Works for both VIFF and PLY file dialogs

### 9614da4 - Use red tint overlay for unselected ROI areas
- Previous: Unselected pixels darkened to 25% (hard to see details)
- New: 70% original brightness + 30% red tint
- Preserves detail visibility while marking unselected areas

### 66c143c - Close all image windows when main window is closed
- Main window close event now closes all child image windows

---

## Architecture Notes

### Key Files

| File | Purpose |
|------|---------|
| `src/MainWindow.cpp` | Application main window, file management |
| `src/ImageWindow.cpp` | Individual image display window with ROI tools |
| `src/DepthImageView.cpp` | Image rendering (color mapping, Graycast) |
| `src/registration/RegistrationWorker.cpp` | ICP algorithms (4-DOF and 6-DOF) |
| `src/registration/CoarseRegistration.cpp` | COM and landmark-based alignment |
| `src/dialogs/MatchingControlPanel.cpp` | Registration UI controls |
| `src/io/ViffReader.cpp` | VIFF file format parser |

### Transformation Model

```
4-DOF (Align):
  p' = Rz(alpha) · p + (tx, ty, tz)

6-DOF (Refine):
  p' = Rz(alpha) · Ry(beta) · Rx(gamma) · p + (tx, ty, tz)
```

### Difference Image Color Scheme

- **Red**: Negative values (data below reference, material loss)
- **Black**: Zero (surfaces equal)
- **White**: Positive values (data above reference, material gain)

---

## Future Improvements (Saved Prompts)

### Automatic All/ROI Mode Switching

**Context:** Match3D v2 has two view modes controlled by radio buttons in ImageWindow:
- **All**: Shows all pixels at full brightness (no overlay/shading)
- **ROI**: Shows only selected pixels, unselected pixels are black

**Current behavior:** User must manually switch between modes. Unselected areas have a red tint overlay when in "All" mode during ROI editing.

**Requested behavior:** Implement automatic mode switching to eliminate the need for the red tint overlay:

1. **During ROI editing (polygon/rectangle/ellipse drawing):**
   - Automatically switch to "All" mode when user starts drawing
   - Full visibility, no shading needed

2. **After completing a selection:**
   - Automatically switch to "ROI" mode after the final click/closing of shape
   - This is triggered in `ImageWindow::finalizeRoiShape()` or equivalent

3. **On "Unselect All" / "Clear ROI":**
   - Automatically switch to "All" mode
   - Triggered in `ImageWindow::clearRoi()` or `RoiMask::clear()`

4. **On "Invert" selection:**
   - Stay in current mode (ROI)

**Files to modify:**
- `src/ImageWindow.cpp` - ROI editing logic, mode switching
- `src/ImageWindow.h` - if new methods needed
- `src/DepthImageView.cpp` - remove red tint overlay code (revert to simple black for unselected in ROI-only mode)

**Key functions to find:**
- `setRoiOnly()` - switches between All/ROI modes
- `finalizeRoiShape()` or polygon closing logic
- `clearRoi()` or ROI clear button handler
- Radio button handlers for All/ROI

**After implementation:** Remove the red tint blending code from `DepthImageView::rebuildImage()` and `rebuildGrayCast()` - restore simple division or black pixels for unselected areas since the user will never see unselected pixels with shading (automatic mode switching handles it).

---

## Technical References

1. Neugebauer, P.J. (1997). "Geometrical cloning of 3D objects via simultaneous registration of multiple range images."
2. Chen, Y. and Medioni, G. (1992). "Object modelling by registration of multiple range images." (Point-to-plane ICP)
3. Horn, B.K.P. (1987). "Closed-form solution of absolute orientation using unit quaternions."
4. Besl, P.J. and McKay, N.D. (1992). "A Method for Registration of 3-D Shapes." (Original ICP)
