# Match3D+ — User Manual: ROI Selection, Histogram, Statistics, and Gradient Clipping

Version: 2026-05-17

---

## 1. ROI Selection

The Region of Interest (ROI) defines which pixels are included in processing operations and in ICP registration. All pixels start as *selected* (in the ROI). Selection operations mark pixels as included or excluded without modifying the underlying depth data.

### 1.1 Viewing the Selection

The toolbar at the top of each image window contains two radio buttons:

| Button | Effect |
|--------|--------|
| **All** | All valid pixels are shown at full brightness. Excluded pixels are shown at 25% brightness (dark but visible). |
| **ROI** | Only selected pixels are shown at full brightness. Excluded pixels are shown as black. |

### 1.2 Polygon Selection

The polygon tool lets you draw a freehand boundary and select or unselect the enclosed area.

**To select a region with a polygon:**
1. Open **Edit → Select polygon** (or **Unselect polygon** to exclude an area).
2. The status bar shows: *"left-click to add vertices — right-click to close — Backspace to undo last point — Esc to cancel"*
3. **Left-click** anywhere in the image to place a vertex. A red line connects the vertices as you draw.
4. **Backspace** removes the last placed vertex (you can undo multiple vertices one by one).
5. **Right-click** to finish — the right-click position is added as the final vertex and the polygon is closed. All pixels inside the polygon are selected (or unselected).
6. **Esc** cancels the polygon without making any change.

A polygon requires at least 3 vertices before it can be closed.

**Load / Save polygon:**
- **Edit → Load polygon...** loads a previously saved polygon from a text file and applies it as a selection.
- Use **Edit → Save polygon...** (if available) to save the current polygon vertices for reuse.

The polygon file format is plain text, one vertex per line: `col row` (floating-point image coordinates).

### 1.3 Strip and Ellipse Selection

| Menu item | Effect |
|-----------|--------|
| Edit → Select/Unselect horiz. strip | Select or exclude a horizontal band of rows |
| Edit → Select/Unselect vert. strip | Select or exclude a vertical band of columns |
| Edit → Select/Unselect ellipse | Select or exclude an elliptical region (center + radii in pixels) |

Each opens a small dialog for the required parameters.

### 1.4 Bulk Operations

| Menu item | Effect |
|-----------|--------|
| Edit → Select all | Mark every pixel as selected |
| Edit → Unselect all | Mark every pixel as excluded |
| Edit → Select complement | Invert the current selection (selected ↔ excluded) |

### 1.5 Commit Selection

**Edit → Commit selection** permanently zeroes out all excluded pixels in the depth image (sets their Z value to 0, making them invalid). The selection is then reset to "all selected". This is irreversible — use with care.

### 1.6 ROI and ICP Registration

When you run ICP via the Matching Control Panel, only pixels that are *selected* in both the model image and the data image are used. This lets you focus the registration on a specific anatomical region (e.g. a single cusp) and exclude surrounding areas that might disturb the alignment.

---

## 2. Clipping Operations

Clipping automatically unselects pixels that fall outside a specified range. Unlike filters, clipping never modifies Z values — it only changes the selection mask.

### 2.1 Clip to Z Range

**Edit → Clip to Z range** opens a dialog with two fields:

| Field | Description |
|-------|-------------|
| Z min | Lower bound; pixels with Z < min are unselected |
| Z max | Upper bound; pixels with Z > max are unselected |

The initial values are taken from the current display clip range shown in the image window. You can also apply the clip range directly from the Histogram dialog (see Section 3.3).

### 2.2 Clip to Gradient

Laser scanners measure steeply angled surfaces less accurately. **Edit → Clip to gradient...** removes pixels in regions where the local surface slope exceeds a specified angle from the XY plane.

**How it works:**
- For each selected pixel, the Z difference to its four 4-connected neighbors is compared to a threshold.
- The threshold is computed per axis from the physical pixel size and the chosen angle:
  - X-direction: `thX = xPixelSize × tan(angle)`
  - Y-direction: `thY = yPixelSize × tan(angle)`
- If any neighbor differs by more than the axis threshold, the pixel is unselected.

**Dialog:**

| Field | Range | Default | Description |
|-------|-------|---------|-------------|
| Max slope angle (from XY plane) | 1° – 89° | 45° | Surface slopes steeper than this angle are removed |

**Examples:**
- **45°** — removes surfaces rising or falling by more than one pixel-width of Z per pixel step. A good general starting point.
- **30°** — more aggressive; keeps only relatively flat regions.
- **60°** — conservative; only removes very steep walls.

After applying, switch to **ROI** display mode to inspect which pixels were removed.

---

## 3. Histogram

The histogram shows the distribution of depth values (Z) across all currently *selected* pixels within the current display clip range.

**Open with:** Z Range → Histogram...

### 3.1 Reading the Histogram

- **Horizontal axis:** Z value, from the current clip minimum (left) to clip maximum (right). The exact values are printed below the axis.
- **Vertical axis:** Percentage of total valid pixels. The peak percentage is printed at the top left; 0% at the bottom left.
- **White bars:** Each bar represents the count of selected pixels in that Z bin. 256 bins are used.
- **Red label "L: set min"** (top left): left-click sets the lower clip boundary.
- **Blue label "R: set max"** (top right): right-click sets the upper clip boundary.

### 3.2 Adjusting the Clip Range Interactively

- **Left-click** anywhere in the histogram area to set the **lower** clip boundary to the Z value at that horizontal position. The image updates immediately.
- **Right-click** anywhere to set the **upper** clip boundary. The image updates immediately.
- The histogram rebuilds after each click to reflect the new range.

The clip range set here is the same range used by Z Range → Set global Z range and by **Clip to Z range**.

### 3.3 Clip to Z Range Button

The **"Clip to Z range"** button at the bottom of the histogram dialog unselects all ROI pixels whose Z value falls outside the current histogram clip range \[min, max\]. This is equivalent to running Edit → Clip to Z range with the histogram's current boundaries.

### 3.4 Saving the Histogram

**File → Save...** in the histogram dialog saves the bin data as a tab-separated text file:

```
# match3d histogram
# ZMin = <value>
# ZMax = <value>
# TotalValidPixels = <count>
# Format: z_center  count  percent
<z_center>	<count>	<percent>
...
```

One row per bin (256 rows), with the Z value at the bin center, the raw pixel count, and the percentage of total valid pixels.

---

## 4. Statistics

**Process → Statistics...** computes descriptive statistics for all currently *selected* valid pixels.

### 4.1 Output Fields

```
Image           = <filename>
Date            = <ISO date/time>
ValidPixels     = <count>
Min             = <value>
Max             = <value>
Mean            = <value>
StdDev          = <value>
Q02             = <value>
Q05             = <value>
Q10             = <value>
Q50             = <value>   (median)
Q90             = <value>
Q95             = <value>
Q98             = <value>
# Volume (mm³) - for difference/subtracted images
PixelArea       = <value> mm²
PosPixels       = <count>
NegPixels       = <count>
PosVolume       = <value> mm³
NegVolume       = <value> mm³
```

**Why these quantiles?**
Laser scanner data often contains outliers at the extremes. The 2nd and 98th percentiles give robust bounds that exclude 2% of outliers on each side. The 5th/95th and 10th/90th percentiles offer progressively more conservative bounds. The median (50th percentile) is a robust measure of the central depth.

### 4.2 Volume Calculations

The volume fields are particularly useful for **difference images** and **surface-subtracted images** (after Fit Plane or Fit Sphere subtraction):

| Field | Description |
|-------|-------------|
| PixelArea | Area of one pixel: xPixelSize × yPixelSize (mm²) |
| PosPixels | Number of pixels with z > 0 |
| NegPixels | Number of pixels with z < 0 |
| PosVolume | Total volume above zero: Σ(z × pixelArea) for z > 0 (mm³) |
| NegVolume | Total volume below zero: Σ(\|z\| × pixelArea) for z < 0 (mm³) |

**Interpretation for wear analysis:**
- After subtracting a fitted plane or sphere, the surface should be approximately at z = 0
- **NegVolume** represents material loss (wear volume) - the volume of the depression below the reference surface
- **PosVolume** represents material above the reference (protrusions, or noise)
- For a well-fitted reference, PosVolume should be small compared to NegVolume

### 4.2 Saving Statistics

Click **Save...** in the statistics dialog to write the same text to a `.txt` file. The format (`Key = Value`, one value per line) is designed to be read by automated scripts. Lines starting with `#` are comments.

---

## 5. Display Styles

The **Style** dropdown in the toolbar controls how depth values are mapped to pixel colors. **Graycast** is the default style.

| Style | Description |
|-------|-------------|
| Linear | Full grayscale from clip min (black) to clip max (white). Uses the full data range. |
| False color | Negative values → red (darker = more negative); positive values → gray (brighter = more positive). Each half-range is normalized independently. Zero = black. |
| Medium gray | Constant medium gray for all valid pixels; useful for checking coverage |
| Linear±3*SD | Linear grayscale mapping using mean ± 3×standard deviation as the range. Values outside this range are clipped to black/white. This suppresses outliers and provides better contrast for the main data distribution. |
| Graycast | **(Default)** Grayscale with exaggerated local shading to reveal surface curvature. Flat surfaces appear dark, slopes appear bright. |

**False color** is intended for difference images (result of subtracting one scan from another). Red areas indicate material loss (negative Z difference), gray areas indicate material gain or no change.

**Linear±3*SD** is useful when data contains extreme outliers that would otherwise compress the useful dynamic range. By mapping mean ± 3σ to the full grayscale range, approximately 99.7% of the data is visible with good contrast.

---

## 6. Slice View (1D Cross-Section)

The slice feature displays a one-dimensional cross-section through the depth image along an arbitrary line. This is useful for measuring height profiles, step heights, and surface roughness along a specific path.

### 6.1 Creating a Slice

1. Open **Process → Show Slice...**
2. The status bar shows: *"Slice: left-click to set start point, right-click to set end point — Esc to cancel"*
3. **Left-click** anywhere in the image to set the start point (a cyan dot appears)
4. Move the mouse to see a dashed cyan preview line
5. **Right-click** to set the end point and create the slice
6. Press **Esc** to cancel without creating a slice

The slice line can have any orientation — horizontal, vertical, or diagonal.

### 6.2 Reading the Slice Window

The Slice window displays the depth profile along the selected line:

- **Horizontal axis (X):** Distance along the slice in millimeters, from 0 (start point) to the total slice length (end point)
- **Vertical axis (Y):** Interpolated Z values (depth) at each position along the slice
- **White line:** The depth profile. Gaps indicate invalid pixels along the slice path.

Z values are computed using bilinear interpolation from the four surrounding pixels at each sample point.

### 6.3 Measuring with the Slice

**Coordinate display:**
- Move the mouse over the slice plot to see coordinates in the status bar
- Coordinates are shown as `x=... mm  y=...` (distance along slice, depth value)

**Relative measurements:**
1. **Left-click** on the slice plot to set a reference point (yellow crosshair marker)
2. **Right-click** to toggle between absolute and relative coordinate display
3. In relative mode, coordinates are shown as `dx=... mm  dy=...` (distance and height difference from the reference point)

This makes it easy to measure:
- Step heights between two points
- Distance between features
- Depth of grooves or height of ridges

### 6.4 Saving Slice Data

**File → Save...** exports the slice profile as a tab-separated text file:

```
# Match3D+ slice data
# Start point (col, row): 100.0, 150.0
# End point (col, row): 300.0, 150.0
# Slice length: 2.500 mm
# Format: distance_mm  z_value
0.000000	1.234567
0.012500	1.234890
...
```

This format can be imported into spreadsheet software or plotting programs for further analysis.

---

## 7. Auto-Matching for Wear Measurement

### 7.1 The Problem with Manual Reference Selection

Traditional wear measurement without fixed reference points (brackets, markers) requires selecting areas on the tooth surface that have not changed between measurements. These could be fissures or non-occluding enamel areas. However:

- Large restorations often leave only small enamel reference areas
- Small reference areas may have subtle changes that go undetected
- If registration is based on changed reference areas, the calculated wear will be incorrect
- This can manifest as impossible "material gain" appearing in parts of the difference image

### 7.2 The Auto-Matching Solution

**Auto-Matching** uses a-priori knowledge: wear only removes material — it never adds material. After correct registration, the follow-up model can only be "below" the baseline model (negative Z difference = material loss), never "above" it.

The algorithm implements this by:
1. Defining a noise threshold (default: 5 µm)
2. During each ICP iteration, excluding correspondences where `diff < -threshold` (i.e., "material gain" beyond noise)
3. This allows wear (positive differences) while filtering impossible "material gain" outliers
4. The final result has no significant "material gain" areas, only actual wear

### 7.3 Using Auto-Matching

1. Open the Matching Control Panel (**Match → Parameters...**)
2. Check the **Auto-Matching** checkbox
3. Adjust the **Threshold** if needed (default 0.005 mm = 5 µm corresponds to sensor noise)
4. Run **Align** (4-DOF) or **Refine** (6-DOF)
   - Note: CCCoreLib ICP does not support Auto-Matching
5. The status bar indicates when Auto-Matching is active

### 7.4 When to Use Auto-Matching

| Situation | Recommendation |
|-----------|----------------|
| Large reference areas available | Manual ROI selection (Schmelz-Matching) may be sufficient |
| Small or uncertain reference areas | Use Auto-Matching |
| Difference image shows unexpected "material gain" | Re-run with Auto-Matching |
| Very noisy data | Increase the threshold slightly |

### 7.5 Technical Details

The threshold should be set to approximately the sensor noise level:
- Too low: May over-constrain the registration
- Too high: Won't effectively suppress false positive outliers
- Typical value: 5 µm (0.005 mm) for laser scanner data

Auto-Matching is applied during the ICP correspondence search. Points where `baseline_z - followup_z < -threshold` (i.e., follow-up appears above baseline by more than the noise threshold) are excluded from the optimization. This forces the algorithm to iteratively adjust until no "material gain" outliers remain beyond the noise level.

---

## 8. Typical Workflow: ROI-Based Registration

1. Open the model image and the data image.
2. In the model image, draw a polygon around the region of interest (**Edit → Select polygon**). Check the result with the **ROI** radio button.
3. Optionally, run **Edit → Clip to gradient...** (45°) to remove steep walls that the scanner measured poorly.
4. Optionally open the **Histogram** and adjust the Z clip range to exclude depth outliers, then click **Clip to Z range**.
5. Repeat steps 2–4 for the data image.
6. Open the Matching Control Panel (**Match → Parameters...**), select the model, and run ICP. Only the selected pixels in both images will be used.
7. After registration, use **Process → Statistics...** on the resulting difference image to evaluate wear.
