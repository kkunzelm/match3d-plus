# Surface Fitting — Technical Documentation

Version: 2026-05-19

---

## 1. Overview

This document describes the algorithms used for plane and sphere fitting in match3d_v2. Both algorithms use least-squares optimization to find the best-fit geometric primitive to a set of 3D points derived from the selected ROI of a depth image.

**Source files:**
- `src/ImageProcessor.h` — Data structures and function declarations
- `src/ImageProcessor.cpp` — Algorithm implementations

---

## 2. Coordinate System

Points are extracted from the ViffImage in **world coordinates**:

```
x = col × xPixelSize    (horizontal position in mm)
y = row × yPixelSize    (vertical position in mm)
z = image.at(row, col)  (depth value in mm)
```

Where:
- `row` ∈ [0, rows-1] is the image row index
- `col` ∈ [0, cols-1] is the image column index
- `xPixelSize`, `yPixelSize` are the physical pixel dimensions (typically in mm)

---

## 3. Plane Fitting Algorithm

### 3.1 Mathematical Model

The plane is parameterized as:

```
z = A·x + B·y + C
```

Where A, B, C are the coefficients to be determined.

### 3.2 Least Squares Formulation

Given n points (xᵢ, yᵢ, zᵢ), we minimize the sum of squared residuals:

```
E = Σᵢ (zᵢ - A·xᵢ - B·yᵢ - C)²
```

Setting ∂E/∂A = ∂E/∂B = ∂E/∂C = 0 yields the normal equations:

```
M · [A, B, C]ᵀ = b
```

Where the 3×3 matrix M and vector b are:

```
M = | Σx²   Σxy   Σx  |      b = | Σxz |
    | Σxy   Σy²   Σy  |          | Σyz |
    | Σx    Σy    n   |          | Σz  |
```

### 3.3 Solution Method

The 3×3 system is solved using **Cramer's rule**:

```cpp
det(M) = M₀₀(M₁₁M₂₂ - M₁₂M₂₁) - M₀₁(M₁₀M₂₂ - M₁₂M₂₀) + M₀₂(M₁₀M₂₁ - M₁₁M₂₀)

A = det(Mₐ) / det(M)   // Mₐ: column 0 replaced with b
B = det(Mᵦ) / det(M)   // Mᵦ: column 1 replaced with b
C = det(Mᶜ) / det(M)   // Mᶜ: column 2 replaced with b
```

### 3.4 Quality Metrics

**RMS Error:**
```
RMS = √(Σᵢ(zᵢ - A·xᵢ - B·yᵢ - C)² / n)
```

**Slope angles (degrees):**
```
Slope_X = atan(A) × 180/π
Slope_Y = atan(B) × 180/π
```

### 3.5 Plane Subtraction

For each pixel (row, col) with valid depth z:
```
z_result = z - (A·x + B·y + C)
```

The result is stored in a new ViffImage with `isDiffImage = true` to indicate that zero/negative values are valid.

### 3.6 Requirements

- Minimum 3 non-collinear points required
- Points should span the image area for a well-conditioned system
- Singular matrix (det ≈ 0) indicates collinear points

---

## 4. Sphere Fitting Algorithm

### 4.1 Mathematical Model

The sphere is parameterized as:

```
(x - h)² + (y - k)² + (z - l)² = r²
```

Where (h, k, l) is the center and r is the radius.

### 4.2 Nonlinear Least Squares Formulation

Given n points (xᵢ, yᵢ, zᵢ), we minimize:

```
E = Σᵢ (rᵢ - r)²
```

Where rᵢ = √((xᵢ-h)² + (yᵢ-k)² + (zᵢ-l)²) is the distance from point i to the center.

This is a nonlinear optimization problem solved using the **Gauss-Newton method**.

### 4.3 Gauss-Newton Iteration

**Residual:**
```
dᵢ = rᵢ - r
```

**Jacobian (n × 4 matrix):**
```
J[i,:] = [-(xᵢ-h)/rᵢ,  -(yᵢ-k)/rᵢ,  -(zᵢ-l)/rᵢ,  -1]
```

**Update step:**
Solve the normal equations:
```
(JᵀJ) · δ = Jᵀ · (-d)
```

Where δ = [δh, δk, δl, δr]ᵀ is the parameter update.

**Parameter update:**
```
h ← h + δh
k ← k + δk
l ← l + δl
r ← r + δr
```

### 4.4 Initial Estimate

**Center:** Centroid of all points
```
h₀ = (Σxᵢ) / n
k₀ = (Σyᵢ) / n
l₀ = (Σzᵢ) / n
```

**Radius:** Average distance from centroid
```
r₀ = (Σ√((xᵢ-h₀)² + (yᵢ-k₀)² + (zᵢ-l₀)²)) / n
```

### 4.5 Convergence Criterion

Iteration continues until:
```
|g_new - g_old| < 10⁻¹⁰
```

Where g = Σ|Jᵀd| is the gradient norm.

Maximum iterations: 100

### 4.6 Linear System Solution

The 4×4 normal equations (JᵀJ)δ = Jᵀ(-d) are solved using **Gaussian elimination with partial pivoting**:

1. Form augmented matrix [JᵀJ | Jᵀ(-d)]
2. Forward elimination with row pivoting
3. Back substitution

### 4.7 Orientation Detection

After convergence, the sphere orientation is determined by comparing the center z-coordinate to the average z of the data points:

```cpp
convex = (l > z_mean)  // true if center is above data
```

- **Convex (above):** Scanner looking down at a ball
- **Concave (below):** Scanner looking into a cavity

### 4.8 Quality Metrics

**RMS Error:**
```
RMS = √(Σᵢ(rᵢ - r)² / n)
```

**Iterations:** Number of Gauss-Newton iterations until convergence

### 4.9 Sphere Subtraction

For each pixel (row, col) with valid depth z:

1. Compute horizontal distance to center:
   ```
   Δx = x - h
   Δy = y - k
   d_xy² = Δx² + Δy²
   ```

2. If d_xy² < r² (point is within sphere's XY projection):
   ```
   z_sphere = l ± √(r² - d_xy²)
   ```
   Use + for convex (sphere above), - for concave (sphere below)

3. Subtract:
   ```
   z_result = z - z_sphere
   ```

Points outside the sphere's XY projection are left unchanged.

### 4.10 Requirements

- Minimum 4 non-coplanar points required
- Points should span a reasonable arc of the sphere
- Better coverage leads to more accurate center and radius estimates

---

## 5. Data Structures

### 5.1 PlaneFit

```cpp
struct PlaneFit {
    double A = 0;           // Coefficient for x
    double B = 0;           // Coefficient for y
    double C = 0;           // Constant term
    double rmsError = 0;    // RMS residual error
    uint32_t pointCount = 0;
    bool valid = false;
};
```

### 5.2 SphereFit

```cpp
struct SphereFit {
    double h = 0;           // Center x coordinate
    double k = 0;           // Center y coordinate
    double l = 0;           // Center z coordinate
    double radius = 0;      // Sphere radius
    double rmsError = 0;    // RMS residual error
    uint32_t pointCount = 0;
    int iterations = 0;     // Gauss-Newton iterations
    bool convex = true;     // true=above (z+), false=below (z-)
    bool valid = false;
};
```

---

## 6. API Reference

### 6.1 Plane Functions

```cpp
// Fit a plane to selected ROI pixels
static PlaneFit fitPlane(const ViffImage& img, const RoiMask* roi);

// Subtract fitted plane from image, returns new image
static ViffImage subtractPlane(const ViffImage& img, const PlaneFit& fit);

// Format fit results as text for display/saving
static QString formatPlaneFit(const PlaneFit& fit, const QString& imageLabel);
```

### 6.2 Sphere Functions

```cpp
// Fit a sphere to selected ROI pixels
static SphereFit fitSphere(const ViffImage& img, const RoiMask* roi);

// Subtract fitted sphere from image, returns new image
static ViffImage subtractSphere(const ViffImage& img, const SphereFit& fit);

// Format fit results as text for display/saving
static QString formatSphereFit(const SphereFit& fit, const QString& imageLabel);
```

---

## 7. References

1. Eberly, D. "Least Squares Fitting of Data." Geometric Tools, LLC.
   https://www.geometrictools.com/

2. Quammen, C. "Nonuniform Background Removal" ImageJ plugin.
   Original plane fitting implementation.

3. Kunzelmann, K.-H. "FitPlaneCubicSphere" ImageJ plugin (2012).
   Sphere fitting implementation using Gauss-Newton method.

---

## 8. Numerical Considerations

### 8.1 Precision

- All intermediate calculations use `double` precision
- Final results stored as `float` in ViffImage for compatibility
- Convergence tolerance: 10⁻¹⁰ for sphere fitting

### 8.2 Robustness

- Singular matrix detection (det < 10⁻¹⁵) for plane fitting
- Maximum iteration limit (100) for sphere fitting
- Skip degenerate cases (rᵢ < 10⁻¹²) in Jacobian computation

### 8.3 Performance

- Single-pass accumulation for normal equation matrices
- No dynamic memory allocation in inner loops
- Typical execution: < 100ms for 500×500 image on modern hardware
