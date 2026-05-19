# Match3D+ — User Manual: Surface Fitting (Plane & Sphere)

Version: 2026-05-19

---

## 1. Overview

The Surface Fitting feature allows you to fit geometric primitives (planes or spheres) to 3D scan data and subtract them to isolate surface deformations or wear. This is useful for:

- **Flat sample wear analysis**: Fit a plane to the unworn reference surface, then subtract it to reveal and measure wear depressions.
- **Spherical antagonist wear analysis**: Fit a sphere to a ball mill ball or similar spherical specimen, then subtract it to measure the wear facet volume.

**Key principle:** The fitting algorithm uses only the *selected* (ROI) pixels. By excluding the deformed/worn area from the ROI before fitting, you ensure the fitted surface represents the ideal reference shape.

---

## 2. Accessing Surface Fitting

**Menu:** Process → Fit Surface → Fit Plane... (or Fit Sphere...)

Both options open a dialog showing the fit results. From this dialog you can:
- View the fit parameters and quality metrics
- Save the results to a text file
- Subtract the fitted surface from the original image, creating a new image window

---

## 3. Fit Plane

### 3.1 When to Use

Use plane fitting for flat specimens where the reference surface is nominally planar. Typical applications:
- Composite resin wear specimens
- Flat ceramic or metal test pieces
- Any surface with localized deformation on an otherwise flat background

### 3.2 Workflow

1. **Load the 3D scan** of the flat specimen.
2. **Exclude the deformed area** using Edit → Unselect polygon (or other ROI tools). Draw around the wear cavity or deformation so it is excluded from the selection.
3. **Verify the selection** by switching to "ROI" display mode. The deformed area should appear dark/excluded; the surrounding flat reference surface should be bright/selected.
4. **Run the fit:** Process → Fit Surface → Fit Plane...
5. **Review the results** in the dialog:
   - **Points**: Number of pixels used for fitting
   - **A, B, C**: Plane coefficients (z = Ax + By + C)
   - **Slope X, Slope Y**: Tilt angles in degrees
   - **RMS Error**: Root-mean-square fit residual (lower is better)
6. **Subtract the plane:** Click "Subtract Plane" to create a new image showing only the deviation from the plane.

### 3.3 Interpreting the Result

After subtraction:
- The flat reference surface will be approximately at z = 0
- Depressions (wear) will show as negative values
- Protrusions will show as positive values
- Use **Process → Statistics...** on the result to measure the wear depth and **wear volume**

**Volume measurement:**
The Statistics dialog now includes volume calculations:
- **NegVolume** = total wear volume (material loss below the fitted plane)
- **PosVolume** = volume above the fitted plane (should be small for a good fit)

### 3.4 Output Parameters

| Parameter | Description |
|-----------|-------------|
| Points | Number of selected valid pixels used for fitting |
| A | Slope coefficient in X direction (mm/mm) |
| B | Slope coefficient in Y direction (mm/mm) |
| C | Plane height at origin (mm) |
| Slope X | Angle of plane tilt in X direction (degrees from horizontal) |
| Slope Y | Angle of plane tilt in Y direction (degrees from horizontal) |
| RMS Error | Root-mean-square distance of points from fitted plane (mm) |

---

## 4. Fit Sphere

### 4.1 When to Use

Use sphere fitting for spherical specimens. Typical applications:
- Ball mill balls used as antagonists in wear simulators
- Spherical dental cusps
- Any surface that should conform to a spherical shape

### 4.2 Workflow

1. **Load the 3D scan** of the spherical specimen.
2. **Exclude the wear facet** using Edit → Unselect polygon. Draw around the flat worn area so it is excluded from the selection.
3. **Verify the selection** by switching to "ROI" display mode. The wear facet should appear dark/excluded; the surrounding spherical surface should be bright/selected.
4. **Run the fit:** Process → Fit Surface → Fit Sphere...
5. **Review the results** in the dialog:
   - **Center (h, k, l)**: Sphere center coordinates in mm
   - **Radius / Diameter**: Fitted sphere dimensions
   - **Orientation**: Whether the sphere is convex (above) or concave (below)
   - **RMS Error**: Root-mean-square fit residual
   - **Iterations**: Number of iterations for convergence
6. **Subtract the sphere:** Click "Subtract Sphere" to create a new image showing only the deviation from the sphere.

### 4.3 Sphere Orientation

The algorithm automatically detects whether the sphere surface is:
- **Convex (above)**: The scanner is looking down at a ball from above. The sphere center is above the surface points.
- **Concave (below)**: The scanner is looking into a spherical cavity. The sphere center is below the surface points.

This is determined by comparing the fitted center Z coordinate to the average Z of the data points.

### 4.4 Interpreting the Result

After subtraction:
- The spherical reference surface will be approximately at z = 0
- The wear facet will show as positive values (material that should be there but is missing)
- Areas outside the sphere's XY projection are set to zero
- Use **Process → Statistics...** on the result to measure wear characteristics

**Volume measurement:**
The Statistics dialog now includes volume calculations:
- **PosVolume** = wear facet volume (material missing from the ideal sphere)
- **NegVolume** = volume below the sphere (should be small for a good fit)

### 4.5 Output Parameters

| Parameter | Description |
|-----------|-------------|
| Points | Number of selected valid pixels used for fitting |
| Center h (X) | X coordinate of sphere center (mm) |
| Center k (Y) | Y coordinate of sphere center (mm) |
| Center l (Z) | Z coordinate of sphere center (mm) |
| Radius | Fitted sphere radius (mm) |
| Diameter | Fitted sphere diameter (mm) |
| Orientation | Convex (above) or Concave (below) |
| Iterations | Number of Gauss-Newton iterations for convergence |
| RMS Error | Root-mean-square distance of points from fitted sphere (mm) |

---

## 5. Best Practices

### 5.1 ROI Selection Tips

- **Exclude generously**: It's better to exclude too much around the deformation than too little. The fitting algorithm needs clean reference surface data.
- **Use multiple exclusion zones**: If there are multiple defects, use Unselect polygon multiple times.
- **Check coverage**: Ensure the selected reference surface covers a sufficient area and is well-distributed. For plane fitting, points should span the image area. For sphere fitting, points should cover a reasonable arc of the sphere.

### 5.2 Quality Indicators

- **RMS Error**: A low RMS error indicates a good fit. For typical dental wear studies:
  - Plane fit: RMS < 0.005 mm is excellent, < 0.010 mm is good
  - Sphere fit: RMS < 0.010 mm is excellent, < 0.020 mm is good
- **Point count**: More points generally give more reliable fits. Minimum is 3 for planes, 4 for spheres.

### 5.3 Troubleshooting

| Problem | Possible Cause | Solution |
|---------|----------------|----------|
| "Not enough points" error | Too few selected pixels | Expand the ROI selection |
| High RMS error | Deformation included in selection | Exclude more area around the defect |
| Poor sphere fit | Points don't span enough arc | Ensure selection covers a larger portion of the sphere |
| Unexpected orientation | Surface normal ambiguity | Check that exclusion region is correct |

---

## 6. Typical Workflow: Flat Sample Wear Analysis

1. Load the 3D scan of the worn flat specimen.
2. **Edit → Select all** to start with everything selected.
3. **Edit → Unselect polygon** — draw around the wear depression.
4. Switch to "ROI" mode to verify the selection looks correct.
5. **Process → Fit Surface → Fit Plane...**
6. Review the fit statistics (check RMS error).
7. Click **"Subtract Plane"** to generate the result image.
8. On the result image, use **Process → Statistics...** to get wear depth statistics.
9. Optionally save the plane fit parameters and statistics for documentation.

---

## 7. Typical Workflow: Spherical Antagonist Wear Analysis

1. Load the 3D scan of the worn ball.
2. **Edit → Select all** to start with everything selected.
3. **Edit → Unselect polygon** — draw around the wear facet (flat worn area).
4. Switch to "ROI" mode to verify the selection looks correct.
5. **Process → Fit Surface → Fit Sphere...**
6. Review the fit statistics (check RMS error and radius).
7. Click **"Subtract Sphere"** to generate the result image.
8. On the result image, the wear facet appears as the deviation from the ideal sphere.
9. Use **Process → Statistics...** to characterize the wear.
10. Optionally save the sphere fit parameters for documentation.

---

## 8. File Format: Saved Fit Results

When you click "Save..." in the fit dialog, the results are saved as a plain text file:

**Plane fit example:**
```
# match3d plane fit
# Image: sample_001.xv
# Date:  2026-05-19 14:30
# Plane equation: z = A*x + B*y + C
Points         = 125000
A              = 1.234567e-04
B              = -2.345678e-04
C              = 0.123456 mm
Slope X        = 0.0071 deg
Slope Y        = -0.0134 deg
RMS Error      = 0.003456 mm
```

**Sphere fit example:**
```
# match3d sphere fit
# Image: ball_001.xv
# Date:  2026-05-19 14:35
# Sphere equation: (x-h)² + (y-k)² + (z-l)² = r²
Points         = 85000
Center h (X)   = 2.456789 mm
Center k (Y)   = 3.567890 mm
Center l (Z)   = 4.678901 mm
Radius         = 2.500123 mm
Diameter       = 5.000246 mm
Orientation    = Convex (above)
Iterations     = 12
RMS Error      = 0.008765 mm
```

---

## 9. Synthetic Test Data

For testing the surface fitting functions, Match3D includes a synthetic data generator that creates VIFF files with known geometric properties.

**Generate a flat sample with spherical depression:**
```bash
./build/src/generate_wear_samples depression test_depression.xv --sphere 0.2
```

**Generate an antagonist with wear facet (truncated dome):**
```bash
./build/src/generate_wear_samples dome test_dome.xv --sphere 0.2 --truncate 0.1
```

See `docs/synthetic-test-data.md` for complete documentation of all generator options and parameters.
