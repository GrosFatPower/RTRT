# Blender Cycles Ground Truth Reference Guide

This guide describes how to use [`Tools/render_test_reference.py`](file:///d:/Users/GRIFFON/Documents/Dev/RTRT/Tools/render_test_reference.py) to replicate any RTRT render test in Blender Cycles and generate photorealistic ground-truth reference images (`.pfm` and `.png`).

---

## 1. Quick Start

### Run via System Python (Automatic Blender Discovery)

```powershell
python Tools/render_test_reference.py --test pathtracer_cornell --compare
```

The script automatically locates `blender.exe` (via standard installation paths or `PATH`), configures the scene, launches Blender in background mode, and compares the rendered image with the current test output.

### Run Directly with Blender

```powershell
& "C:\Program Files\Blender Foundation\Blender 5.2\blender.exe" --background --python Tools/render_test_reference.py -- --test pathtracer_cornell --compare
```

---

## 2. CLI Options

| Option | Description | Default |
| :--- | :--- | :--- |
| `--test <name>` | Name of the test case to replicate (from test manifest) | None |
| `--all` | Replicate all `pathtracer` tests defined in the manifest | `False` |
| `--manifest <path>` | Path to test manifest JSON | `Tests/RenderTests.json` |
| `--samples <int>` | Cycles sample count for ground truth | `256` |
| `--compare` | Compare reference against existing `Tests/Artifacts/<test>/actual.pfm` | `False` |
| `--update-baseline` | Copy generated `reference.pfm` to the test's baseline/reference path | `False` |
| `--output-dir <path>` | Custom directory for output artifacts | `Tests/Artifacts/blender_reference_<test>/` |
| `--blender <path>` | Explicit path to `blender.exe` (when running from system Python) | Auto-detect |

---

## 3. Generated Artifacts

For each replicated test case, the runner writes to `Tests/Artifacts/blender_reference_<test_name>/`:

1. **`reference.pfm`**: 32-bit float linear HDR image matching RTRT's internal [`ReadPFM`](file:///d:/Users/GRIFFON/Documents/Dev/RTRT/Tests/RenderTestImageUtil.cpp#L47-L74) scanline convention (bottom-to-top scanlines).
2. **`reference.png`**: 8-bit diagnostic preview with Reinhard tone mapping \(\frac{x}{1 + x}\) and gamma \(\gamma = 2.2\) matching [`WriteDiagnosticPNG`](file:///d:/Users/GRIFFON/Documents/Dev/RTRT/Tests/RenderTestImageUtil.cpp#L80-L101).
3. **`diff_amplified_5x.png`** *(when `--compare` is enabled)*: 5x amplified absolute difference map vs `actual.pfm`.
4. **`reference_report.json`**: JSON report detailing resolution, sample count, channel bounds, and MAE/RMSE metrics.

---

## 4. Replicated Engine Features

The tool translates RTRT scene concepts directly into Blender Cycles:

- **Manifest Overrides**: Profile defaults and test overrides (`resolution`, `camera`, `environment_map`, `settings`) are resolved identically to [`RenderRegression`](file:///d:/Users/GRIFFON/Documents/Dev/RTRT/Tests/RenderRegressionMain.cpp).
- **Coordinate Conventions**:
  - RTRT uses $+Y$ up, $+Z$ depth.
  - OBJ meshes are imported with identity mapping (`forward_axis='Y', up_axis='Z'`).
  - glTF assets are converted with an exact inverse rotation to align with RTRT's $+Y$-up convention.
- **Camera Mapping**:
  - Position, pivot/lookat, and FOV are translated into Blender's camera world matrix:
    \[
    M_{cam} = \begin{bmatrix} R_x & U_x & -F_x & P_x \\ R_y & U_y & -F_y & P_y \\ R_z & U_z & -F_z & P_z \\ 0 & 0 & 0 & 1 \end{bmatrix}
    \]
  - Horizontal sensor fit is configured to preserve exact horizontal FOV across varying aspect ratios.
  - Orthographic projections (`type orthographic`) map directly to Cycles orthogonal cameras.
- **PBR Materials**:
  - Base color, perceptual roughness, metallic, IOR, and transmission map 1:1 to Blender's `Principled BSDF`.
  - Texture maps (`albedotexture`, `metallicroughnesstexture`, `normaltexture`, `emissiontexture`) are automatically bound with correct color spaces.
- **Light Types**:
  - **Quad Light**: Built from corner vertices, with two-sided emission and transparent backface to eliminate self-shadowing against ceilings/walls. Camera ray visibility is disabled matching `u_ShowLights == 0`.
  - **Sphere Light**: Converted into physical diffuse sphere lights using radiant power \(\Phi = 4\pi^2 R^2 L_e\).
  - **Distant Light**: Converted into directional Sun lights.
  - **Environment Maps**: Equirectangular HDR textures rotated $+90^\circ$ around X to align the vertical pole with RTRT $+Y$-up.

---

## 5. Workflow Examples

### Replicating a Test and Inspecting Difference

```powershell
python Tools/render_test_reference.py --test pathtracer_cornell --compare
```

### Promoting a Verified Blender Reference to Regression Baseline

```powershell
python Tools/render_test_reference.py --test pathtracer_cornell --update-baseline
```

This copies the verified ground truth `.pfm` directly into `Tests/Baselines/pathtracer_cornell.pfm` (or the test's configured baseline path), establishing a new regression baseline.

### Generating References for All Path Tracer Tests

```powershell
python Tools/render_test_reference.py --all --compare
```
