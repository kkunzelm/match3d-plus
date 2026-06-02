# Match3D+: STL Import User Manual

This guide explains how to import STL files from intraoral scanners and convert them to 2.5D heightmaps for analysis in Match3D+.

## Table of Contents

1. [Overview](#overview)
2. [Quick Start](#quick-start)
3. [Opening STL Files](#opening-stl-files)
4. [The Import Dialog](#the-import-dialog)
   - [3D Preview](#3d-preview)
   - [2D Projection Preview](#2d-projection-preview)
   - [Quick Alignment Buttons](#quick-alignment-buttons)
   - [Fine Rotation Controls](#fine-rotation-controls)
   - [Projection Settings](#projection-settings)
5. [Orienting the Mesh](#orienting-the-mesh)
6. [Projection to Heightmap](#projection-to-heightmap)
7. [Tips and Best Practices](#tips-and-best-practices)

---

## Overview

Match3D+ can import 3D mesh data from STL files (commonly exported by intraoral scanners) and project them onto 2D heightmaps for analysis with the existing registration and difference calculation tools.

```
┌─────────────────┐
│   STL File      │
│  (3D Triangle   │
│     Mesh)       │
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│  Interactive    │
│  Orientation    │
│  (3D Preview)   │
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│  Z-Projection   │
│  to Heightmap   │
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│  VIFF Image     │
│ (2.5D Depth Map)│
└─────────────────┘
```

### Why Project to 2D?

- **Consistent workflow**: Use the same registration and analysis tools for all data
- **Efficient processing**: 2.5D operations are faster than full 3D
- **Dental wear measurement**: Surface changes are primarily in the vertical direction

### Limitations

- **Undercuts lost**: Overhanging geometry is projected to a single Z value per pixel
- **Orientation matters**: The mesh must be oriented so the surface of interest faces upward (+Z)

---

## Quick Start

1. Go to **File → Import STL...**
2. Select an STL file from your intraoral scanner
3. Use the 3D preview to rotate the tooth so the occlusal surface faces up
4. Adjust resolution if needed (default: 0.025 mm/pixel)
5. Click **Import** to create the heightmap
6. The new image appears in a window, ready for registration

---

## Opening STL Files

### Supported Formats

- **Binary STL**: Standard format exported by most scanners (recommended)
- **ASCII STL**: Text-based format (also supported, but larger file sizes)

### How to Import

1. Select **File → Import STL...** from the menu
2. Browse to your STL file and click **Open**
3. The STL Import Dialog opens with the 3D preview

---

## The Import Dialog

The import dialog has two main areas: a 3D preview on the left and a 2D projection preview on the right.

```
┌────────────────────────────────────────────────────────────┐
│  Import STL: tooth_scan.stl                            [X] │
├──────────────────────────────┬─────────────────────────────┤
│                              │  2D Projection Preview      │
│      3D Preview              │  ┌─────────────────────┐    │
│                              │  │                     │    │
│   [Interactive 3D View       │  │  [Grayscale         │    │
│    with rotation]            │  │   heightmap]        │    │
│                              │  │                     │    │
│                              │  └─────────────────────┘    │
│                              │  Size: 512 × 480 px         │
│                              │  Coverage: 87.3%            │
├──────────────────────────────┴─────────────────────────────┤
│  Quick Alignment        Fine Rotation        Projection    │
│  [Top][Bottom]          X: [----●----]       Resolution:   │
│  [Front][Back]          Y: [----●----]       [0.025] mm/px │
│  [90X][90Y][90Z]        Z: [----●----]       [✓] Auto size │
│  [Reset]                                                   │
├────────────────────────────────────────────────────────────┤
│  Mesh Info: Triangles: 125432, Size: 12.5 × 8.3 × 6.2 mm  │
├────────────────────────────────────────────────────────────┤
│                                    [Cancel]    [Import]    │
└────────────────────────────────────────────────────────────┘
```

### 3D Preview

The left panel shows an interactive 3D view of your mesh:

- **Rotate**: Click and drag to rotate the object
- **Zoom**: Use the mouse scroll wheel
- **Reference grid**: A gray XY grid helps visualize the projection plane
- **Axes widget**: Shows the current orientation (Red=X, Green=Y, Blue=Z)

The mesh rotates around its center. The Z-axis (blue) indicates the projection direction.

### 2D Projection Preview

The right panel shows a preview of the resulting heightmap:

- **Grayscale image**: Brighter = higher Z values
- **Size**: Dimensions of the output image in pixels
- **Coverage**: Percentage of pixels that contain valid data

### Quick Alignment Buttons

| Button | Action |
|--------|--------|
| **Top** | View from +Z (looking down at XY plane) |
| **Bottom** | View from -Z (looking up) |
| **Front** | View from +Y direction |
| **Back** | View from -Y direction |
| **90X** | Rotate 90° around X axis |
| **90Y** | Rotate 90° around Y axis |
| **90Z** | Rotate 90° around Z axis |
| **Reset** | Return to original orientation |

### Fine Rotation Controls

Three sliders allow precise rotation around each axis:

- **X rotation**: -180° to +180°
- **Y rotation**: -180° to +180°
- **Z rotation**: -180° to +180°

The sliders update automatically when you rotate the mesh in the 3D view.

### Projection Settings

| Setting | Description |
|---------|-------------|
| **Resolution** | Pixel size in mm/pixel (smaller = finer detail, larger image) |
| **Auto size** | Automatically calculate image dimensions from mesh bounds |

Default resolution is 0.025 mm/pixel (25 µm), suitable for most dental applications.

---

## Orienting the Mesh

### Goal

Position the mesh so that:
1. The surface of interest faces **upward** (+Z direction)
2. The occlusal table (for teeth) is approximately parallel to the XY plane

### Strategy

1. Use **Quick Alignment** buttons to get close to the desired orientation
2. Use **Fine Rotation** sliders or direct 3D manipulation for precise adjustment
3. Check the **2D Projection Preview** to verify the result

### Example: Orienting a Molar

```
Step 1: Initial orientation (often arbitrary from scanner)
        The tooth might be tilted or upside down

Step 2: Click "Top" to view from above
        Observe which surface is facing up

Step 3: Use 90X/90Y buttons to flip if needed
        Get the occlusal surface facing up

Step 4: Fine-tune with sliders
        Align cusp tips to be roughly level
```

### Tips for Good Orientation

- **Minimize tilt**: The occlusal surface should be as horizontal as possible
- **Center the ROI**: The area of interest should be centered in the projection
- **Consistent orientation**: Use the same orientation for all scans in a comparison study

---

## Projection to Heightmap

### How Projection Works

The mesh is projected along the Z-axis onto a 2D grid:

1. Each pixel in the output image corresponds to an (X, Y) location
2. Triangles are rasterized using barycentric interpolation
3. When multiple triangles overlap, the **maximum Z value** is kept (Z-buffer)
4. Pixels with no triangle coverage receive a "no data" value (displayed as black)

```
Side View:          Top View (Projection):

    ▲ Z             ┌───────────────┐
    │               │   ░░░███░░░   │  ░ = low Z
    │  /\           │  ░░████████░  │  █ = high Z
    │ /  \          │ ░██████████░░ │
    │/____\         │  ░░████████░  │
    └──────► X      │   ░░░███░░░   │
                    └───────────────┘
```

### Coverage Percentage

The coverage percentage indicates how much of the output image contains valid data:

- **>90%**: Good coverage, the mesh fills most of the image
- **50-90%**: Acceptable, some empty regions
- **<50%**: Check orientation or increase margin

Low coverage often indicates the mesh is oriented edge-on to the projection direction.

### Resolution Selection

| Resolution | Use Case | Typical Image Size |
|------------|----------|-------------------|
| 0.010 mm | High detail, small samples | 1000+ pixels |
| 0.025 mm | Standard dental work | 400-800 pixels |
| 0.050 mm | Quick preview, large samples | 200-400 pixels |
| 0.100 mm | Very large samples | 100-200 pixels |

Higher resolution (smaller mm/pixel) produces larger images but captures finer detail.

---

## Tips and Best Practices

### Before Import

1. **Clean your STL**: Remove artifacts, floating triangles, or noise in your scanner software
2. **Check file size**: Very large meshes (>1M triangles) may be slow to render
3. **Know your scanner**: Different scanners use different coordinate conventions

### During Import

1. **Start with Quick Alignment**: Use Top/Bottom/Front/Back to get initial orientation
2. **Watch the 2D preview**: It updates in real-time as you rotate
3. **Aim for maximum coverage**: Higher coverage means less data loss
4. **Use consistent orientation**: For comparative studies, orient all scans the same way

### After Import

1. The imported image appears in a new window
2. You can now use all standard Match3D+ features:
   - ROI selection
   - Registration with another scan
   - Difference calculation
   - Statistics and volume measurement

### Troubleshooting

| Problem | Solution |
|---------|----------|
| Black/empty preview | Mesh may be too small or resolution too coarse |
| Mesh not visible in 3D | Check if file loaded correctly (see Mesh Info) |
| Low coverage | Rotate mesh so the surface faces up, not edge-on |
| Jagged edges | Decrease resolution (smaller mm/pixel value) |
| Import very slow | Reduce mesh complexity or use larger resolution |

---

## See Also

- [User Manual: Registration](user-manual-registration.md) - Aligning two scans
- [User Manual: ROI, Histogram, Statistics](user-manual-roi-histogram-statistics.md) - Analyzing the data
- [User Manual: Surface Fitting](user-manual-surface-fitting.md) - Measuring wear
