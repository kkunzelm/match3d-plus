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

### a0d223c - Add development history and update .gitignore
- Created CLAUDE_HISTORY.md for development documentation

### 6d9f317 - Fix 6-DOF registration, add unit conversion, and improve UI
- **6-DOF point-to-plane ICP rewrite**: Fixed Jacobian formulation for proper convergence
  - Proper inverse transform for model→data projection: `P_data = R^T * (P_model - T)`
  - Correct linearized Jacobian: `[(P × n), n]` for parameters (γ, β, α, tx, ty, tz)
  - Relaxed slope filter from 75° to 89° (only filter near-vertical surfaces)
  - Debug counters for troubleshooting registration failures
- **Automatic unit conversion in ViffReader**:
  - Detects dental scan data: pixel sizes in meters, z values in micrometers
  - Auto-converts to millimeters for consistent internal use
  - Heuristic: `pixelSize < 0.001` (meters) AND `zMax > 1000` (µm) triggers conversion
- **Difference image improvements**:
  - Removed ROI filtering from difference calculation (computes ALL pixels)
  - Model's ROI mask is copied to the new difference image for analysis
- **Status bar**: Shows coordinates in mm instead of pixels
- **Save dialogs**: Auto-append `.txt` extension when missing
- **New tool**: `synthetic_test_data` for generating test surfaces with known transformations

### 5191b14 - Add CCCoreLib ICP button
- **New "ICP" button** in Matching Control Panel (next to Align and Refine)
  - Uses `ICPRegistrationTools::Register` from CCCoreLib
  - Full 6-DOF registration (all rotations + translations)
  - Supports outlier filtering, random sampling, and overlap ratio
- Intended to eventually replace the custom "Align" algorithm
- Configuration:
  - `MAX_ITER_CONVERGENCE` mode
  - `filterOutFarthestPoints = true` (helps with partial overlap)
  - `adjustScale = false` (no scale changes for dental scans)

### 05689a6 - Add Surface Fitting (Fit Plane, Fit Sphere)
- **New Process → Fit Surface submenu** with two fitting operations:
  - **Fit Plane**: Least-squares plane fitting (z = Ax + By + C)
  - **Fit Sphere**: Iterative Gauss-Newton sphere fitting
- **Plane fitting algorithm**:
  - Builds 3×3 normal equation system from selected ROI pixels
  - Solves using Cramer's rule
  - Reports coefficients, slope angles, and RMS error
- **Sphere fitting algorithm**:
  - Gauss-Newton iteration with Jacobian-based updates
  - Initial estimate from centroid and average distance
  - Auto-detects convex (above) vs concave (below) orientation
  - Reports center, radius, iterations, and RMS error
- **Subtract operations**: Create new window with fitted surface subtracted
  - Useful for isolating wear depressions on flat samples
  - Useful for measuring wear facets on spherical antagonists
- **Result dialogs**: Show fit statistics with Save and Subtract buttons
- **Documentation**:
  - `docs/user-manual-surface-fitting.md` - User guide with workflows
  - `docs/surface-fitting-technical.md` - Algorithm details and math
- Based on original Java implementation (FitPlaneCubicSphere_.java)

### (current) - Fix sphere fitting, add volume statistics, add wear sample generator
- **Fixed CCCoreLib ICP transformation composition**:
  - Bug: Initial transform was pre-applied to data cloud but not composed with ICP result
  - Fix: Properly compute R_total = R_icp × R0 and T_total = R_icp × T0 + T_icp
- **Fixed sphere fitting algorithm**:
  - Bug: Sign error in Gauss-Newton update caused divergence
  - Fix: Correctly compute J^T × (radius - r) instead of J^T × (r - radius)
  - Now matches the original Java implementation exactly
  - Convergence tolerance: 10⁻⁸, max iterations: 500
- **Fixed sphere subtraction**:
  - Changed from `z_data - z_sphere` to `z_sphere - z_data`
  - Positive values now indicate material missing (wear facet)
  - Points outside sphere projection set to zero
- **Fixed convex/concave auto-detection**:
  - Changed from `l > zMean` to `l < zMean` for correct orientation
- **Added volume calculation to Statistics**:
  - `PosVolume`: Sum of z × pixelArea for z > 0 (mm³)
  - `NegVolume`: Sum of |z| × pixelArea for z < 0 (mm³)
  - `PosPixels` / `NegPixels`: Pixel counts
  - Useful for measuring wear volume after surface subtraction
- **New tool: `generate_wear_samples`**:
  - Generates synthetic VIFF files for testing surface fitting
  - `depression` mode: Flat plane with spherical cavity (wear simulation)
  - `dome` mode: Flat plane with truncated hemisphere (antagonist simulation)
  - Configurable size, pixel size, sphere diameter, truncation, noise
- **Documentation**:
  - `docs/synthetic-test-data.md` - Complete guide for test data generators
  - Updated user manuals with volume calculation documentation

---

## Architecture Notes

### Key Files

| File | Purpose |
|------|---------|
| `src/MainWindow.cpp` | Application main window, file management |
| `src/ImageWindow.cpp` | Individual image display window with ROI tools |
| `src/DepthImageView.cpp` | Image rendering (color mapping, Graycast) |
| `src/ImageProcessor.cpp` | Image processing: filters, statistics, surface fitting |
| `src/tools/GenerateWearSamples.cpp` | Synthetic wear sample generator (depression, dome) |
| `src/registration/RegistrationWorker.cpp` | ICP algorithms (4-DOF, 6-DOF, CCCoreLib ICP) |
| `src/registration/CoarseRegistration.cpp` | COM and landmark-based alignment |
| `src/dialogs/MatchingControlPanel.cpp` | Registration UI controls |
| `src/io/ViffReader.cpp` | VIFF file format parser with auto unit conversion |
| `src/tools/SyntheticTestData.cpp` | Test data generator for registration validation |

### Registration Methods

| Button | Method | DOF | Description |
|--------|--------|-----|-------------|
| From COM | Center of mass | 3 | Translation only (tx, ty, tz) |
| From Points | Landmark-based | 4 | 2D rotation + translation |
| Align | Custom 2.5D ICP | 4 | Z-rotation + translation (α, tx, ty, tz) |
| Refine | Point-to-plane ICP | 6 | Full Euler + translation (α, β, γ, tx, ty, tz) |
| ICP | CCCoreLib ICP | 6 | Standard Besl & McKay ICP with outlier filtering |

### Surface Fitting Methods

| Menu Item | Method | Parameters | Description |
|-----------|--------|------------|-------------|
| Fit Plane | Least-squares | A, B, C | z = Ax + By + C, solved via normal equations |
| Fit Sphere | Gauss-Newton | h, k, l, r | (x-h)² + (y-k)² + (z-l)² = r², iterative |

Both methods use only ROI-selected pixels. Subtract operations create a new image with the fitted surface removed, useful for isolating wear depressions or measuring wear facets.

### Transformation Model

```
4-DOF (Align):
  p' = Rz(alpha) · p + (tx, ty, tz)

6-DOF (Refine/ICP):
  p' = Rz(alpha) · Ry(beta) · Rx(gamma) · p + (tx, ty, tz)
```

### Unit System

All internal calculations use **millimeters (mm)**. ViffReader automatically converts:
- Pixel sizes: meters → mm (×1000)
- Z values: µm → mm (÷1000)

Detection heuristic: `xPixelSize < 0.001` AND `zMax > 1000`

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
