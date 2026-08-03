# Render Regression and Unit Test Framework Plan

## Summary

Add a local CTest-integrated framework that renders deterministic scenes through all three backends, compares the final linear-color image to committed PFM baselines, and produces visual failure artifacts.

## Goals

- Detect visible rendering regressions caused by renderer, shader, material, or lighting changes.
- Provide a repeatable command-line path to load a scene, render a fixed frame sequence, and evaluate its output.
- Provide a lightweight in-repository harness for CPU unit tests without adding a third-party dependency.
- Keep the interactive `RenderLab` workflow unchanged.

## Architecture

### Build targets

- Refactor CMake to build a shared renderer/core target.
- Keep `RenderLab` as the interactive executable.
- Add a `RenderRegression` executable that starts without Test5 or ImGui.
- Enable CTest and register unit and render cases with labels.

### Render runner

- Create a hidden GLFW 4.1 context with `GLFW_VISIBLE` disabled and vsync disabled.
- Initialize GLEW and validate that the required OpenGL context is available.
- Return CTest skip code `77` only when a compatible context cannot be created. Rendering failures, GL errors, or comparison failures remain test failures.
- Reuse the existing `Loader`, scene model, `RenderSettings`, and renderer implementations.
- Factor renderer selection into a shared renderer factory so Test5 and the runner construct the same backend types.

### Renderer readback

- Add a `RenderImage` type containing resolution and linear `RGBA32F` pixels.
- Add `Renderer::ReadbackFinalColor( RenderImage & oImage )`.
- Each renderer reads back its fully composed final-color output at window resolution.
- Keep the existing `RenderToFile()` PNG capture behavior unchanged; it remains a user-facing export feature rather than the regression source.

## Test Data and Comparison

### Files

- Commit linear PFM baselines under `Tests/Baselines/`.
- Store generated failure artifacts under ignored `Tests/Artifacts/<case>/`.
- Define initial render cases in a C++ registry, avoiding a new scene-test parsing format in v1.

### Artifacts

Each failed render case writes:

- `actual.pfm`
- `expected.pfm`
- Tone-mapped `actual.png` and `expected.png`
- Amplified `diff.png`
- `metrics.txt` with mean absolute error, maximum absolute error, and above-threshold pixel ratio

### Baseline policy

- Baselines are created only by the explicit `--update-baselines` command.
- Regular test runs never modify baselines.
- New or updated baselines must be reviewed using the generated PNG artifacts before being committed.
- PFM is the baseline format because it preserves linear float RGB data without adding an image-library dependency. PNG files are diagnostic artifacts only.

### Metrics

- Compare linear float RGB values.
- Compute mean absolute error, maximum absolute error, and the fraction of pixels exceeding a per-case error threshold.
- Use strict thresholds for deterministic software rendering and calibrated tolerances for OpenGL and path-tracing cases.

## Command-Line Interface

`RenderRegression` provides:

- `--list`: list unit and render cases.
- `--case <name>`: run one named case.
- `--all`: run all registered cases.
- `--update-baselines`: allow baseline writes for selected render cases.
- `--artifacts <directory>`: override the default artifact directory.

CTest labels:

- `unit`
- `render`
- `deferred`
- `software`
- `pathtracer`

## Initial Render Cases

### Software rasterizer

- Name: `software_textured_box`
- Scene: `Assets/TexturedBox.scene`
- Resolution: `1280x720`
- Frames: one
- Output: final composited color
- Thresholds: MAE `0.000001`, maximum error `0.00001`, mismatch ratio `0%`

### Deferred OpenGL renderer

- Name: `deferred_ibl_ssr`
- Scene: `Assets/tungsten-material-testball.scene`
- Resolution: `1280x720`
- Settings: specular IBL and SSR enabled
- Frames: warm up two frames, capture frame three
- Output: final composited color
- Thresholds: MAE `0.003`, maximum error `0.08`, pixels above `0.02` limited to `0.5%`

### Cornell cross-backend cases

- Names: `software_cornell`, `deferred_cornell`, and `pathtracer_cornell`
- Scene: `Assets/cornell_box.scene`
- Resolution: `720x720`
- Camera: position `(0.276, 0.265, -0.750)`, pivot `(0.276, 0.265, 0.100)`, FOV `40` degrees, near plane `0.5`
- Software settings: one frame with strict raster thresholds
- Deferred settings: three frames with the deferred tolerance policy

### Dining Room cross-backend cases

- Names: `software_diningroom`, `deferred_diningroom`, and `pathtracer_diningroom`
- Scene: `Assets/diningroom.scene`, using its authored camera
- Resolution: `1280x720`
- Software settings: one frame with strict raster thresholds
- Deferred settings: three frames with the deferred tolerance policy
- Path-tracer settings: six accumulated frames, one sample per pixel, four bounces, and the full-resolution path enabled

### Teapot environment cross-backend cases

- Names: `software_teapot_env`, `deferred_teapot_env`, and `pathtracer_teapot_env`
- Scene: `Assets/teapot.scene`, using its authored camera
- Environment map: `Assets/HDR/Background_05.hdr`, loaded explicitly by the test runner with environment mapping enabled
- Resolution: `1280x720`
- Software settings: one frame with strict raster thresholds
- Deferred settings: three frames with specular IBL and SSR enabled
- Path-tracer settings: six accumulated full-resolution frames, one sample per pixel, four bounces, tiled rendering disabled, and denoising disabled

### Path tracer

- Name: `pathtracer_cornell`
- Scene: `Assets/cornell_box.scene`
- Resolution: `720x720`
- Camera: position `(0.276, 0.265, -0.750)`, pivot `(0.276, 0.265, 0.100)`, FOV `40` degrees, near plane `0.5`
- Settings: six accumulated frames, one sample per pixel, four bounces, accumulation enabled, auto-scale enabled to disable the interactive low-resolution pass, denoising disabled
- Output: final composited color
- Thresholds: MAE `0.02`, maximum error `0.25`, pixels above `0.08` limited to `2%`

## Initial Unit Cases

- PFM read/write and vertical orientation.
- Linear image metric calculations.
- Diff-image generation.
- Render-case validation for missing scenes, invalid resolutions, invalid frame counts, and missing baselines.

## Verification

1. Build `RenderLab` and `RenderRegression` in Debug.
2. Run `ctest -L unit` without creating an OpenGL context.
3. Run `ctest -L render` and confirm each backend matches its committed baseline.
4. Intentionally alter a comparison threshold or baseline pixel and confirm a non-zero exit code plus all expected artifacts.
5. Run `RenderRegression --update-baselines --case deferred_ibl_ssr` and confirm that baseline writes occur only with the explicit flag.
6. Confirm Test5 still uses the shared renderer factory and its interactive output is unchanged.

## Assumptions and Limits

- Version one evaluates final composited color only. G-buffer, BRDF LUT, SSAO, SSR, and other intermediate outputs remain manual debug views until a later extension adds per-pass capture.
- GPU baselines are most reliable on the reference GPU and driver used to approve them. The initial tolerances allow limited variation but do not promise pixel-exact results across all platforms.
- CI and GitHub Actions are intentionally out of scope for the first version. The initial deliverable is local CTest plus the runner CLI.
