# Engineering Design Principles

## Problem Decomposition

The task is to register two 2D synthetic circle images that differ in both **size** and **position**.
This requires a registration approach that can simultaneously recover:

1. A **scale factor** (circle diameters are 30 mm and 60 mm → scale fixed→moving = 2.0)
2. A **translation** (centers are at (50, 50) mm and (200, 200) mm; with scale=2 and
   rotation centre at the origin, the transform equation gives translation = (100, 100) mm)

Because the intended use case extends to at least 20 collaborators applying this tool to millions
of registration tasks, every design decision prioritises **correctness**, **reproducibility**,
**ease of use**, and **long-term maintainability**.

---

## Transform Choice — `itk::Similarity2DTransform`

| Candidate | DOF | Why rejected / chosen |
|-----------|-----|-----------------------|
| `TranslationTransform` | 2 | Cannot recover scale |
| `ScaleTransform` | 2 | Cannot recover translation independently |
| **`Similarity2DTransform`** | **4** (scale, angle, tx, ty) | Handles scale + rotation + translation in 2D; closed-form inverse; well-tested in ITK |
| `AffineTransform` | 6 | Over-parameterised for this problem; harder to initialise; risks shear artefacts |

The `Similarity2DTransform` parameterises as:

```
T(x) = s · R(θ) · (x − c) + c + t
```

where `s` is isotropic scale, `R(θ)` is a 2D rotation matrix, `c` is the centre of rotation,
and `t` is the translation vector.

For this problem the expected solution (fixed → moving) is `s = 2.0`, `θ = 0`,
`t = (100, 100)` mm (with centre `c = (0, 0)`).
This maps the fixed circle centre (50, 50) → 2·(50, 50) + (100, 100) = (200, 200) ✓

---

## Metric Choice — Mean Squares (`itk::MeanSquaresImageToImageMetricv4`)

| Candidate | Notes |
|-----------|-------|
| **Mean Squares** | Optimal for same-modality, same-intensity images; computationally cheap; closed-form gradient |
| Mutual Information | Better for multi-modal; extra hyperparameters (bins, samples) add complexity |
| Normalised Cross-Correlation | Good but slower per iteration |

Because both images are synthetically generated with identical foreground (255) and background (0)
intensities, Mean Squares is the theoretically optimal choice and converges reliably.

---

## Optimizer Choice — `itk::RegularStepGradientDescentOptimizerv4`

| Property | Value | Rationale |
|----------|-------|-----------|
| Maximum step length | 4.0 | Large enough to escape shallow basins quickly |
| Minimum step length | 0.001 | Provides sub-pixel convergence |
| Number of iterations | 300 | Sufficient for well-conditioned similarity problems |
| Relaxation factor | 0.5 | Standard backtracking; prevents oscillation |

Regular Step Gradient Descent is deterministic, requires no random seeds, and produces
reproducible results across platforms — a key requirement when sharing results with 20+
collaborators.

---

## Multi-Resolution Strategy

A three-level Gaussian pyramid is used:

| Level | Shrink | Smooth σ (mm) | Purpose |
|-------|--------|---------------|---------|
| 0 (coarsest) | 4× | 2.0 | Capture large-scale displacement quickly |
| 1 | 2× | 1.0 | Refine scale and translation |
| 2 (finest) | 1× | 0.0 | Sub-pixel accuracy |

Multi-resolution registration is essential here because the initial displacement (≈212 mm) is
large relative to the image field of view. Without coarse-to-fine, a gradient descent optimizer
operating at full resolution would require thousands of iterations or diverge entirely.

---

## Initialisation Strategy

The transform is initialised to the **identity** (scale=1, angle=0, translation=(0,0)),
with the rotation centre left at the **origin** (0,0). Keeping the centre at the origin
ensures a clean physical interpretation of the recovered translation parameter:

```
T(x) = scale * x + translation
```

No manual initialisation is needed. The multi-resolution pyramid
handles the large initial offset.

For downstream tasks where better initial alignment is available (e.g., from metadata or a prior
registration), the initial transform parameters can be supplied on the command line
(`--initial-transform`). This makes the tool composable in larger pipelines.

---

## Image Generation Design

Test images are generated analytically (no file I/O dependency):

```
pixel(i,j) = 255  if  distance(physical_point(i,j), center) ≤ radius
           =   0  otherwise
```

Images are stored in **MetaImage (.mha) format** with full spatial metadata
(origin, spacing, direction cosines) to ensure correct physical-space registration regardless of
voxel size. The `.mha` format is human-readable, ITK-native, and does not require external
libraries.

---

## Software Engineering Decisions

| Decision | Choice | Rationale |
|----------|--------|-----------|
| Language | C++17 | Required by ITK v6; best ITK ecosystem support |
| Build system | CMake 3.16+ | Standard for ITK projects; cross-platform |
| Image format | MetaImage (.mha) | Lightweight, ITK-native, stores full spatial metadata |
| Observer pattern | `itk::Command` | Non-intrusive iteration logging; zero cost when disabled |
| Error handling | `itk::ExceptionObject` | Consistent with ITK conventions; descriptive messages |
| Command-line interface | Positional arguments | Simple; avoids third-party CLI libraries |

---

## Limitations and Future Work

- **No rotation initialisation**: If images are rotated >45°, additional initialisation logic is
  needed (e.g., moments-based or exhaustive search).
- **Binary images only**: The Mean Squares metric performs poorly on noisy or multi-modal data.
  Swap in `itk::MattesMutualInformationImageToImageMetricv4` for general use.
- **2D only**: Extending to 3D requires replacing `Similarity2DTransform` with
  `itk::Similarity3DTransform` and updating image type aliases.
- **No GPU acceleration**: For very large images, `itk::GPUImageRegistrationMethodv4` can
  provide significant speedups.
