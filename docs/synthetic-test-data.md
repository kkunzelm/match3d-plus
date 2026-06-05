# Synthetic Test Data Generators

Version: 2026-05-19

---

## Overview

Match3D+ includes command-line tools for generating synthetic 3D surface data. These tools create VIFF files with known geometric properties for testing registration algorithms and surface fitting functions.

**Tools:**
- `synthetic_test_data` — Generates non-symmetric surfaces with known transformations for ICP testing
- `generate_wear_samples` — Generates wear simulation samples for surface fitting testing

---

## 1. Wear Sample Generator (`generate_wear_samples`)

Generates synthetic depth images that simulate typical wear analysis scenarios.

### 1.1 Building

```bash
cd build
cmake ..
make generate_wear_samples
```

The executable is located at `build/src/generate_wear_samples`.

### 1.2 Usage

```bash
generate_wear_samples <type> <output.xv> [options]
```

**Types:**
- `depression` — Flat plane with spherical depression (simulates flat sample wear)
- `dome` — Flat plane with truncated hemisphere (simulates antagonist wear facet)

**Options:**

| Option | Default | Description |
|--------|---------|-------------|
| `--size <n>` | 256 | Image size in pixels (square) |
| `--pixel <f>` | 0.05 | Pixel size in mm |
| `--height <f>` | 1.0 | Plane height in mm |
| `--sphere <f>` | 0.2 | Sphere diameter as fraction of image width |
| `--truncate <f>` | 0.1 | Truncation ratio for dome (fraction of radius to cut) |
| `--noise <f>` | 0.002 | Gaussian noise standard deviation in mm |

### 1.3 Sample Type: Depression

Generates a flat plane with a spherical cap depression in the center. This simulates a flat wear specimen where material has been removed, creating a concave wear cavity.

**Geometry:**
```
Cross-section (side view):

    Plane level ─────────────────────────────
                      ╲         ╱
                       ╲       ╱
                        ╲     ╱
                         ╲   ╱
                          ╲ ╱  ← Spherical depression
                           V
```

**Example:**
```bash
./generate_wear_samples depression flat_sample.xv \
    --size 300 --pixel 0.05 --sphere 0.25 --noise 0.003
```

This creates:
- 300×300 pixel image (15×15 mm at 50 µm/pixel)
- Sphere diameter: 25% of width = 3.75 mm
- Depression depth at center: 1.875 mm (= radius)
- Gaussian noise: 3 µm standard deviation

**Testing workflow:**
1. Open in Match3D
2. Use **Edit → Unselect polygon** to exclude the depression area
3. Run **Process → Fit Surface → Fit Plane...**
4. Click "Subtract Plane" to isolate the depression
5. Use **Process → Statistics...** to measure the depression depth

### 1.4 Sample Type: Dome (Truncated Hemisphere)

Generates a flat plane with a hemisphere (dome) rising from the center, with the top portion cut off flat. This simulates a spherical antagonist (e.g., ball mill ball) that has developed a wear facet.

**Geometry:**
```
Cross-section (side view):

                      ┌─────┐  ← Truncated top (wear facet)
                     ╱       ╲
                    ╱         ╲
                   ╱           ╲
                  ╱             ╲
    Plane level ─╱───────────────╲─────────
```

**Truncation parameter:**
- `--truncate 0.1` means the top 10% of the sphere radius is removed
- If radius = R, dome height = R × (1 - truncate) = 0.9R

**Example:**
```bash
./generate_wear_samples dome antagonist.xv \
    --size 300 --pixel 0.05 --sphere 0.2 --truncate 0.15 --noise 0.002
```

This creates:
- 300×300 pixel image (15×15 mm at 50 µm/pixel)
- Sphere diameter: 20% of width = 3 mm, radius = 1.5 mm
- Full dome height would be: 1.5 mm
- Truncation: 15%, so dome height = 1.5 × 0.85 = 1.275 mm
- The flat top represents the wear facet

**Testing workflow:**
1. Open in Match3D
2. Use **Edit → Unselect polygon** to exclude the flat top (wear facet)
3. Run **Process → Fit Surface → Fit Sphere...**
4. Verify the fitted radius matches the expected value
5. Click "Subtract Sphere" to isolate the wear facet
6. Use **Process → Statistics...** to measure the facet depth

### 1.5 Parameter Guidelines

**Pixel size:**
- Typical dental scanner resolution: 0.02–0.10 mm
- Use 0.05 mm (50 µm) for realistic simulations

**Sphere size:**
- 0.2 (20%) gives a moderate-sized feature
- 0.3–0.4 for larger features that are easier to visualize
- 0.1–0.15 for smaller, more challenging features

**Noise:**
- Typical scanner noise: 0.001–0.005 mm (1–5 µm)
- Use 0.002 mm for realistic simulations
- Use 0.01 mm or higher to test robustness

**Truncation (dome only):**
- 0.05–0.10 for subtle wear facets
- 0.15–0.25 for pronounced wear facets
- 0.30+ for severely worn antagonists

---

## 2. Registration Test Data Generator (`synthetic_test_data`)

Generates pairs of surfaces with known transformations for testing ICP registration algorithms.

### 2.1 Building

```bash
cd build
cmake ..
make synthetic_test_data
```

### 2.2 Usage

```bash
synthetic_test_data <model.xv> <data.xv> [options]
```

**Options:**

| Option | Default | Description |
|--------|---------|-------------|
| `--size <n>` | 256 | Image size in pixels |
| `--pixel <f>` | 0.1 | Pixel size in mm |
| `--alpha <deg>` | 3.0 | Rotation around Z axis |
| `--beta <deg>` | 1.5 | Rotation around Y axis |
| `--gamma <deg>` | 2.0 | Rotation around X axis |
| `--tx <mm>` | 5.0 | Translation in X |
| `--ty <mm>` | -3.0 | Translation in Y |
| `--tz <mm>` | 0.5 | Translation in Z |
| `--noise <mm>` | 0.05 | Noise standard deviation |
| `--seed <n>` | 42 | Random seed |

### 2.3 Generated Surface

The model surface is a non-symmetric combination of:
- Tilted base plane
- Two asymmetric Gaussian bumps at different locations
- A diagonal ridge
- A depression in one corner

This non-symmetry ensures that registration algorithms cannot converge to incorrect local minima due to surface symmetry.

### 2.4 Transformation Convention

The transformation parameters specify the **data→model** transform:
```
P_model = Rz(alpha) × Ry(beta) × Rx(gamma) × P_data + (tx, ty, tz)
```

After successful registration, the recovered parameters should match the input parameters.

### 2.5 Example

```bash
./synthetic_test_data model.xv data.xv \
    --alpha 5.0 --beta 2.0 --gamma 1.5 \
    --tx 8.0 --ty -4.0 --tz 0.3 \
    --noise 0.08
```

**Testing workflow:**
1. Open both files in Match3D
2. Select model.xv, then Ctrl+click data.xv to select both
3. Open **Match → Parameters...** (Matching Control Panel)
4. Click "From COM" for initial alignment
5. Click "Align" or "Refine" for ICP registration
6. Compare recovered parameters to the known input values

---

## 3. File Format

All generators output standard VIFF/XV files:
- 1024-byte header
- 32-bit float depth data (row-major)
- Pixel sizes stored in header

Files can be opened directly in Match3D+ or processed with the VIFF I/O library.

---

## 4. Source Code

| Tool | Source File |
|------|-------------|
| `generate_wear_samples` | `src/tools/GenerateWearSamples.cpp` |
| `synthetic_test_data` | `src/tools/SyntheticTestData.cpp` |

---

## 5. Quick Reference

**Generate flat sample with wear depression:**
```bash
./generate_wear_samples depression wear_sample.xv --sphere 0.2
```

**Generate antagonist with wear facet:**
```bash
./generate_wear_samples dome antagonist.xv --sphere 0.2 --truncate 0.1
```

**Generate registration test pair:**
```bash
./synthetic_test_data model.xv data.xv --alpha 3 --tx 5 --noise 0.05
```
