# Match3D+: Registration User Manual

This guide explains how to use the registration features in Match3D+ to align two 3D surface scans and compute their difference.

## Table of Contents

1. [Overview](#overview)
2. [Quick Start](#quick-start)
3. [Loading Images](#loading-images)
4. [Setting the Region of Interest (ROI)](#setting-the-region-of-interest-roi)
5. [Coarse Registration Methods](#coarse-registration-methods)
   - [From COM (Center of Mass)](#from-com-center-of-mass)
   - [From Points (Landmark-Based)](#from-points-landmark-based)
6. [Fine Registration](#fine-registration)
   - [Align (4-DOF)](#align-4-dof)
   - [Refine (6-DOF)](#refine-6-dof)
7. [Computing the Difference Image](#computing-the-difference-image)
8. [Understanding the Results](#understanding-the-results)
9. [Tips and Troubleshooting](#tips-and-troubleshooting)

---

## Overview

Match3D+ is designed to compare two 3D surface scans (heightmaps) by:

1. **Aligning** the scans so corresponding surface points overlap
2. **Computing** the height difference at each point
3. **Visualizing** the differences as a color-coded image

```
┌─────────────┐     ┌─────────────┐
│  Reference  │     │    Data     │
│   (Model)   │     │   Image     │
└──────┬──────┘     └──────┬──────┘
       │                   │
       │   ┌───────────┐   │
       └──►│ Register  │◄──┘
           └─────┬─────┘
                 │
                 ▼
         ┌─────────────┐
         │ Difference  │
         │   Image     │
         └─────────────┘
```

### Terminology

- **Reference Image (Model)**: The "target" scan that stays fixed
- **Data Image**: The scan that gets transformed to match the reference
- **ROI (Region of Interest)**: A selected area to focus the registration on
- **Transform**: The rotation and translation applied to align the data to the reference

---

## Quick Start

1. Open two images: **File → Open** (select Model/Reference, then Data)
2. Select matching regions in both images using the ROI tools
3. Click **from COM** for initial alignment
4. Click **Align** to refine the alignment (4-DOF)
5. Optionally click **Refine** for final polish (6-DOF)
6. Click **Diff Image** to see the differences

---

## Loading Images

### Opening VIFF Images

1. Go to **File → Open** or use the toolbar
2. Select a VIFF (.xv) file containing 3D depth data
3. The image will open in a new window

### Importing STL Files

If your data comes from an intraoral scanner (STL format):

1. Go to **File → Import STL...**
2. Orient the mesh in the 3D preview (occlusal surface facing up)
3. Click **Import** to create a heightmap

See [User Manual: STL Import](user-manual-stl-import.md) for detailed instructions.

### Designating Reference and Data

In the **Matching Control Panel**:

- **Model/Reference** dropdown: Select the image that serves as the reference (this one won't move)
- **Data** dropdown: Select the image that will be aligned to the reference

```
┌─ Matching Control Panel ─────────────────┐
│                                          │
│  Model:  ▼ [surface_scan_1.xv       ]   │
│  Data:   ▼ [surface_scan_2.xv       ]   │
│                                          │
└──────────────────────────────────────────┘
```

---

## Setting the Region of Interest (ROI)

The ROI limits the registration to a specific area. This is useful when:

- Only part of the surface overlaps between scans
- You want to exclude noisy or irrelevant regions
- You want to focus on a specific feature

### ROI Tools

| Tool | Description |
|------|-------------|
| **Polygon** | Draw a custom polygon shape |
| **Rectangle** | Draw a rectangular selection |
| **Ellipse** | Draw an elliptical selection |
| **Invert** | Swap selected/unselected areas |
| **Clear** | Remove the ROI (use entire image) |

### Tips for ROI Selection

- Select the **same anatomical region** in both images
- Avoid areas with holes (invalid pixels)
- Include distinctive features for better alignment

```
    ┌─────────────────────┐
    │  ████████████       │
    │  █ Selected █       │  ← ROI includes the
    │  █   Area   █       │    overlapping region
    │  ████████████       │
    │                     │
    └─────────────────────┘
```

---

## Coarse Registration Methods

Coarse registration provides an initial alignment. You should do this before running the fine registration.

### From COM (Center of Mass)

**Best for:** Images that are already roughly aligned, or when you just need translation.

**What it does:**
- Computes the center of mass of both ROIs
- Translates the data image so its center matches the reference
- **No rotation** is applied

**How to use:**
1. Set the ROI in both images (or leave empty to use entire image)
2. Click **from COM**
3. The transform fields will update with tx, ty, tz values

```
    Reference               Data                  After COM
    ┌───────┐              ┌───────┐              ┌───────┐
    │   ●   │              │       │              │   ●   │
    │ (COM) │       +      │   ●   │      =       │ ● ●   │
    │       │              │       │              │       │
    └───────┘              └───────┘              └───────┘
                        (shifted to match)
```

### From Points (Landmark-Based)

**Best for:** Images that need rotation alignment, or when you have identifiable landmarks.

**What it does:**
- Uses corresponding landmark points you've picked in both images
- Computes optimal rotation (around Z-axis) and translation
- Computes Z-offset from the median of point differences

**How to use:**
1. Pick at least 3 corresponding point pairs:
   - In the **Reference** image: Shift+Click on landmarks
   - In the **Data** image: Shift+Click on the same landmarks in the same order
2. Click **from Points**
3. The transform fields will update (alpha for rotation, tx/ty/tz for translation)

```
    Reference                Data
    ┌───────┐               ┌───────┐
    │ 1   2 │               │   1   │   Points must be
    │       │               │     2 │   picked in the
    │   3   │               │ 3     │   same order!
    └───────┘               └───────┘
```

**Important:**
- Pick points in the **same order** in both images
- Use easily identifiable features (corners, peaks, distinctive shapes)
- Avoid points in areas with holes or noise

---

## Fine Registration

After coarse registration, use fine registration to improve the alignment.

### Align (4-DOF)

The **Align** button performs 4-DOF (degrees of freedom) refinement:
- **alpha** (rotation around Z-axis)
- **tx, ty, tz** (translation)

**Best for:** Most 2.5D heightmap data where surfaces are roughly parallel.

**What it does:**
1. Finds overlapping pixels based on (X, Y) grid position
2. Iteratively computes optimal 2D rotation and translation
3. Z-offset computed as median of differences (robust to outliers)

**How to use:**
1. First apply a coarse registration (from COM or from Points)
2. Click **Align**
3. A progress bar shows the refinement progress
4. When done, the transform values are updated

```
    After Coarse              After Align
    ┌───────────┐            ┌───────────┐
    │  ░░████░░ │            │  ████████ │
    │  ░████░░░ │    ──►     │  ████████ │  Better aligned!
    │  ░░░░░░░░ │            │  ████████ │
    └───────────┘            └───────────┘
```

### Refine (6-DOF)

The **Refine** button performs 6-DOF point-to-plane refinement:
- **alpha, beta, gamma** (all three rotation angles)
- **tx, ty, tz** (translation)

**Best for:** Final polish when surfaces may have slight tilts, or for maximum precision.

**What it does:**
1. Uses Neugebauer's point-to-plane algorithm
2. Computes surface normals from gradients
3. Minimizes point-to-plane distances (faster convergence than point-to-point)
4. Solves all 6 parameters simultaneously

**How to use:**
1. First run **Align** to get close to the solution
2. Click **Refine** for final optimization
3. Check that beta and gamma remain small (< 1°) for typical 2.5D data

**Note:** If beta and gamma become large, the data may have significant tilt that needs investigation.

### Parameters

| Parameter | Default | Description |
|-----------|---------|-------------|
| **Max Iterations** | 8000 | Maximum refinement steps (usually converges much sooner) |
| **Sampling Limit** | 50000 | Max pixels used per iteration (subsampled for speed) |
| **Min RMS Decrease** | 1e-5 | Convergence threshold (stops when improvement is tiny) |

---

## Computing the Difference Image

After registration, you can compute and visualize the height differences.

### How to use:

1. Complete the registration (coarse + Align + optional Refine)
2. Click **Diff Image**
3. A new window opens showing the difference image

The difference image automatically opens with **FalseColor** display style for optimal visualization. You can change the display style via the View menu if needed.

### What the colors mean:

The difference image uses a **red-black-white** color scale:

| Color | Value | Meaning |
|-------|-------|---------|
| **Red** | Negative | Data surface is **below** reference (material loss/wear) |
| **Black** | Zero | Surfaces are **equal** |
| **White** | Positive | Data surface is **above** reference (material gain) |
| **Transparent** | Invalid | Hole, out of bounds, or outside ROI |

```
    Difference Image Color Scale

    ◄─────────────────────────────────────►
    Red           Black           White
    (below)       (equal)         (above)
    -max            0             +max

    Example: Dental wear study
    ┌─────────────────────────────┐
    │  ░░░░░████░░░░░░░░░░░░░░░  │  Red areas = enamel loss
    │  ░░░██████████░░░░░░░░░░░  │  White areas = deposits
    │  ░░░░░░████░░░░░░░░░░░░░░  │  Black = no change
    └─────────────────────────────┘
```

---

## Understanding the Results

### Transform Values

After registration, the Matching Control Panel shows:

| Field | Description |
|-------|-------------|
| **alpha (α)** | Rotation around Z-axis in degrees |
| **beta (β)** | Rotation around Y-axis (typically ~0 for 2.5D data) |
| **gamma (γ)** | Rotation around X-axis (typically ~0 for 2.5D data) |
| **tx** | Translation in X direction (meters) |
| **ty** | Translation in Y direction (meters) |
| **tz** | Translation in Z direction (depth units) |

### Statistics

After computing the difference image, statistics are displayed:

- **Valid Pixels**: Number of pixels where difference could be computed
- **Mean**: Average difference (positive = data above reference)
- **Std Dev**: Standard deviation of differences
- **Min/Max**: Extreme difference values
- **Quantiles**: Q2, Q5, Q10, Q50, Q90, Q95, Q98 percentiles

---

## Tips and Troubleshooting

### Registration seems wrong

- **Check point order**: For "from Points", landmarks must be picked in the same order
- **Use ROI**: Select only the overlapping region
- **Avoid holes**: Don't pick landmarks in areas with missing data

### Difference image shows all invalid (black/transparent)

- The registration may have failed
- Check that both images actually overlap
- Try a simpler coarse registration (from COM) first

### Large rotation angle (alpha)

- For similar scans, alpha should be small (< 5°)
- Large angles may indicate picked points are in wrong order
- Or the images are rotated relative to each other in the scan

### Large beta or gamma after Refine

- For 2.5D heightmaps, beta and gamma should be near zero
- Large values (> 1°) may indicate:
  - Surface tilt between measurements
  - Mounting differences
  - Algorithm divergence (try fewer iterations)

### Align doesn't improve alignment

- The coarse registration may already be optimal
- Try adjusting the ROI to include more distinctive features
- Increase sampling limit for more precise refinement

### Performance is slow

- Reduce the ROI size
- Lower the sampling limit
- The algorithm samples pixels randomly, so fewer samples = faster but slightly less accurate

---

## Workflow Summary

```
┌─────────────────────────────────────────────────────────────┐
│                     REGISTRATION WORKFLOW                    │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  1. LOAD IMAGES                                             │
│     ├── Open Reference (Model) image                        │
│     └── Open Data image                                     │
│                                                             │
│  2. SELECT ROI (optional but recommended)                   │
│     ├── Draw ROI on Reference image                         │
│     └── Draw matching ROI on Data image                     │
│                                                             │
│  3. COARSE REGISTRATION (choose one)                        │
│     ├── from COM ──► Translation only (simple)              │
│     └── from Points ──► Rotation + Translation (precise)    │
│                                                             │
│  4. FINE REGISTRATION                                       │
│     ├── Align ──► 4-DOF refinement (recommended first)      │
│     └── Refine ──► 6-DOF point-to-plane (optional polish)   │
│                                                             │
│  5. COMPUTE RESULTS                                         │
│     └── Diff Image ──► Visualize height differences         │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

---

## Keyboard Shortcuts

| Action | Shortcut |
|--------|----------|
| Add landmark point | Shift + Click |
| Pan view | Middle mouse drag |
| Zoom | Mouse wheel |

---

*Match3D+ - Qt6/C++20 Implementation*
*For technical details, see `registration-algorithm-technical.tex`*
