# Match3D v2

A Qt6/C++20 application for comparing 3D surface scans by aligning them and computing height differences. Originally developed for dental wear studies, it can be used for any application requiring precise 2.5D surface comparison.

## Purpose

Match3D v2 enables researchers to:

- **Load and visualize** 2.5D depth images (heightmaps) from laser scanners
- **Register (align)** two surfaces using various ICP algorithms
- **Compute difference images** showing surface changes between scans
- **Fit geometric primitives** (planes, spheres) to isolate and measure wear
- **Calculate volume statistics** for quantitative wear analysis

### Dental Wear Analysis Workflow

The primary use case is comparing dental surface scans taken at different time points:

1. **Scan acquisition**: Obtain 3D surface scans of teeth or dental materials using a laser scanner
2. **Coarse alignment**: Use landmark points or center-of-mass to roughly align the surfaces
3. **Fine registration**: Apply ICP algorithms optimized for 2.5D data to achieve precise alignment
4. **Difference calculation**: Subtract aligned surfaces to reveal wear patterns
5. **Quantification**: Use statistics and volume calculations to measure material loss

### Surface Fitting for Wear Measurement

For specimens with known geometry:

- **Flat samples**: Fit a plane to the unworn reference area, subtract it to isolate wear depressions
- **Spherical antagonists**: Fit a sphere to a ball specimen, subtract it to measure wear facet volume

## Features

- Multiple visualization styles: Linear grayscale, false color, Graycast shading
- Region of Interest (ROI) selection: Polygon, rectangle, ellipse, strips
- Gradient clipping to exclude steep surfaces with poor scan quality
- Interactive histogram with adjustable Z-range clipping
- Comprehensive statistics including percentiles and volume calculations
- Export to PLY format for use in other 3D applications

## Dependencies

- **Qt 6.4+** (Widgets, Concurrent modules)
- **CMake 3.20+**
- **C++20 compatible compiler** (GCC 11+, Clang 13+, MSVC 2019+)
- **CCCoreLib** (included as git submodule) - Point cloud library from CloudCompare
- **happly** (included) - PLY file I/O header-only library

## Building

### Clone with Submodules

```bash
git clone --recursive https://github.com/your-username/match3d_v2.git
cd match3d_v2
```

If you already cloned without `--recursive`:

```bash
git submodule update --init --recursive
```

### Compile

```bash
mkdir build && cd build
cmake ..
make -j$(nproc)
```

### Run

```bash
./src/match3d_v2
```

### Utility Tools

The build also produces utility executables:

```bash
# Generate synthetic test data for registration validation
./src/synthetic_test_data

# Generate wear samples (flat+depression or dome+facet) for testing surface fitting
./src/generate_wear_samples --help
```

## File Format

Match3D v2 uses the **VIFF (Visualization Image File Format)** for storing 2.5D depth images. VIFF files (`.xv` extension) contain:

- Single-band floating-point depth values
- Pixel size metadata for physical coordinates
- Compatible with Khoros/VisiQuest software

PLY export is available for interoperability with other 3D software.

## Documentation

Detailed documentation is available in the `docs/` directory:

| Document | Description |
|----------|-------------|
| [User Manual: Registration](docs/user-manual-registration.md) | Complete guide to surface alignment |
| [User Manual: ROI, Histogram, Statistics](docs/user-manual-roi-histogram-statistics.md) | Selection tools and data analysis |
| [User Manual: Surface Fitting](docs/user-manual-surface-fitting.md) | Plane and sphere fitting workflows |
| [Technical: Surface Fitting](docs/surface-fitting-technical.md) | Algorithm details and mathematics |
| [Technical: Registration Strategies](docs/registration-strategies.md) | ICP algorithm documentation |
| [Synthetic Test Data](docs/synthetic-test-data.md) | Guide to test data generators |
| [VIFF Format](docs/viff-format.md) | File format specification |

## Background

Match3D v2 is a modern re-implementation of Match3D 2.5, originally developed by Wolfram Gloger at LMU Munich for dental wear research. The original software was written in C and ran on SGI IRIX workstations.

This version brings the core functionality to modern platforms using Qt6 and C++20, with improved algorithms and additional features for surface fitting and volume measurement.

## Author

**Prof. Dr. Karl-Heinz Kunzelmann**
Department of Conservative Dentistry and Periodontology
LMU Munich

Original author of the surface fitting algorithms and this Qt6/C++20 re-implementation.

## License

This project uses CCCoreLib from CloudCompare, which is licensed under GPL v2+. See `extern/CCCoreLib/LICENSE.txt` for details.

## Acknowledgments

- **Wolfram Gloger** - Original Match3D 2.5 implementation
- **CloudCompare team** - CCCoreLib point cloud library
- **happly** - PLY file I/O library
