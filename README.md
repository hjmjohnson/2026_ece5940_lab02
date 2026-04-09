# ECE 5940 Lab 02 — 2D Circle Image Registration with ITK v6

## Overview

This lab assignment guides you through building a 2D medical image registration program using the
[Insight Toolkit (ITK)](https://itk.org/) version 6 or greater. You will generate synthetic test
images of circles with different sizes and positions, then register them using ITK's image
registration framework.

## Learning Objectives

- Understand the ITK v6 image registration pipeline (metric, optimizer, transform)
- Generate synthetic 2D images programmatically with ITK
- Apply a **Similarity2D** transform to recover scale and translation between two circle images
- Interpret registration results quantitatively and communicate them to collaborators

## Problem Statement

Given two 2D images of circles:

| Image | Diameter | Center (mm) |
|-------|----------|-------------|
| Fixed (`circle_fixed.mha`) | 30 mm | (50, 50) |
| Moving (`circle_moving.mha`) | 60 mm | (200, 200) |

Write a registration program that aligns the moving image to the fixed image and recovers the
underlying geometric transform (scale factor ≈ 0.5, translation ≈ (−100, −100) mm).

## Repository Layout

```
2026_ece5940_lab02/
├── README.md                      # This file
├── EngineeringDesignPrinciples.md # Design rationale and algorithm choices
├── ExperimentalResults.md         # Quantitative validation results
├── CMakeLists.txt                 # CMake build system (requires ITK ≥ 6)
└── src/
    ├── GenerateCircleImages.cxx   # Generates the two synthetic test images
    └── RegisterCircles.cxx        # Main registration program
```

## Prerequisites

| Tool | Minimum Version |
|------|----------------|
| ITK  | 6.0 |
| CMake | 3.16 |
| C++ compiler | C++17 support (GCC ≥ 9, Clang ≥ 9, MSVC 2019) |

ITK must be built (or installed) with at least the following modules enabled:
`ITKCommon`, `ITKIOImageBase`, `ITKIOMHA`, `ITKRegistrationMethodsv4`,
`ITKMetricsv4`, `ITKOptimizersv4`, `ITKTransform`.

## Building

```bash
mkdir build && cd build
cmake -DITK_DIR=/path/to/ITK-build ..
cmake --build . --parallel
```

## Running

### Step 1 — Generate the test images

```bash
./GenerateCircleImages circle_fixed.mha circle_moving.mha
```

This produces:
- `circle_fixed.mha` — 256 × 256 px (0.5 mm/px), 30 mm diameter circle at (50, 50) mm
- `circle_moving.mha` — 512 × 512 px (0.5 mm/px), 60 mm diameter circle at (200, 200) mm

### Step 2 — Run the registration

```bash
./RegisterCircles circle_fixed.mha circle_moving.mha registered_output.mha
```

The program prints the recovered **Similarity2D** transform parameters
(scale, angle, translation) and writes the resampled moving image to
`registered_output.mha`.

### Expected output

```
=== Registration Result ===
Scale        :  2.0000  (expected 2.0000)
Angle (deg)  :  0.0000  (expected 0.0000)
Translation  : [100.00, 100.00] mm  (expected [100.00, 100.00] mm)
Metric value :  <small negative number>
```

## Design and Results

- [EngineeringDesignPrinciples.md](EngineeringDesignPrinciples.md) — why these algorithms were chosen
- [ExperimentalResults.md](ExperimentalResults.md) — accuracy, robustness, and performance data

## License

This code is released under the Apache 2.0 License. See the ITK project for full licensing details.
