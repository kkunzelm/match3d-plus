# Match3D v2: Registration User Manual

This guide explains how to use the registration features in Match3D v2 to align two 3D surface scans and compute their difference.

## Table of Contents

1. [Overview](#overview)
2. [Quick Start](#quick-start)
3. [Loading Images](#loading-images)
4. [Setting the Region of Interest (ROI)](#setting-the-region-of-interest-roi)
5. [Coarse Registration Methods](#coarse-registration-methods)
   - [From COM (Center of Mass)](#from-com-center-of-mass)
   - [From Points (Landmark-Based)](#from-points-landmark-based)
6. [Fine Registration (Match)](#fine-registration-match)
7. [Computing the Difference Image](#computing-the-difference-image)
8. [Understanding the Results](#understanding-the-results)
9. [Tips and Troubleshooting](#tips-and-troubleshooting)

---

## Overview

Match3D v2 is designed to compare two 3D surface scans (heightmaps) by:

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
4. Click **Match** to refine the alignment
5. Click **Diff Image** to see the differences

---

## Loading Images

### Opening Images

1. Go to **File → Open** or use the toolbar
2. Select a VIFF (.xv) file containing 3D depth data
3. The image will open in a new window

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

Coarse registration provides an initial alignment. You must do this before running the fine registration (Match).

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

## Fine Registration (Match)

The **Match** function refines the coarse alignment using an iterative algorithm.

### What it does:

1. Finds overlapping pixels between the aligned images
2. Iteratively adjusts rotation and translation to minimize differences
3. Stops when the alignment converges or reaches max iterations

### Parameters

| Parameter | Default | Description |
|-----------|---------|-------------|
| **Max Iterations** | 8000 | Maximum refinement steps (usually converges much sooner) |
| **Sampling Limit** | 50000 | Max pixels used per iteration (subsampled for speed) |
| **Min RMS Decrease** | 1e-5 | Convergence threshold (stops when improvement is tiny) |

### How to use:

1. First apply a coarse registration (from COM or from Points)
2. Click **Match**
3. A progress bar shows the refinement progress
4. When done, the transform values are updated

```
    After Coarse              After Match
    ┌───────────┐            ┌───────────┐
    │  ░░████░░ │            │  ████████ │
    │  ░████░░░ │    ──►     │  ████████ │  Better aligned!
    │  ░░░░░░░░ │            │  ████████ │
    └───────────┘            └───────────┘
    (slight misalignment)    (refined alignment)
```

---

## Computing the Difference Image

After registration, you can compute and visualize the height differences.

### How to use:

1. Complete the registration (coarse + optional Match)
2. Click **Diff Image**
3. A new window opens showing the difference image

### What the colors mean:

| Color | Meaning |
|-------|---------|
| **Blue** | Data surface is **below** reference |
| **Green/Gray** | Surfaces are **equal** (within tolerance) |
| **Red/Yellow** | Data surface is **above** reference |
| **Black/Transparent** | Invalid pixel (hole, out of bounds, outside ROI) |

```
    Difference Image Color Scale

    ◄─────────────────────────────────────►
    Blue        Green/Gray        Red/Yellow
    (below)      (equal)          (above)
    -max           0              +max
```

---

## Understanding the Results

### Transform Values

After registration, the Matching Control Panel shows:

| Field | Description |
|-------|-------------|
| **alpha (α)** | Rotation around Z-axis in degrees |
| **beta (β)** | Rotation around Y-axis (always 0 for 2.5D data) |
| **gamma (γ)** | Rotation around X-axis (always 0 for 2.5D data) |
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

### Difference image shows all invalid (black)

- The registration may have failed
- Check that both images actually overlap
- Try a simpler coarse registration (from COM) first

### Large rotation angle (alpha)

- For similar scans, alpha should be small (< 5°)
- Large angles may indicate picked points are in wrong order
- Or the images are rotated relative to each other in the scan

### Match doesn't improve alignment

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
│     └── Match ──► Iterative refinement                      │
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

*Match3D v2 - Qt6/C++20 Implementation*
*For technical details, see `registration-algorithm-technical.tex`*
