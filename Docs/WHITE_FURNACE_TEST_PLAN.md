# White Furnace Energy-Conservation Test Plan

## Summary

Add an explicit, non-default white-furnace suite for the deferred renderer and path tracer. It renders a single calibrated sphere probe under a uniform linear-white environment with per-test inline material overrides, measures HDR output numerically, and reports energy violations without inter-probe reflection or occlusion.

## Implementation Changes

- Add a committed `64x32` linear Radiance HDR asset containing constant `(1,1,1)` radiance. Use the existing environment-map loader; no production environment API changes are needed.
- Add `Assets/WhiteFurnace.scene`, auto-discovered by Test5:
  - Orthographic camera (`orthographicHeight 2.5`) framing a single centered sphere at `(0, 0, 0)`, visible white environment background (physical furnace walls), no local lights.
  - Default `furnace_probe` material overridden per test case in the manifest.
- Add `Tests/RenderTests.WhiteFurnace.json`, separate from `RenderTests.json`:
  - `12` `deferred_white_furnace_*` cases: one frame at `256x256`.
  - `12` `pathtracer_white_furnace_*` cases: `256` accumulated frames at `256x256`, one sample per pixel, two bounces, no denoising, auto-scaling, or tile rendering.
  - Presets per backend:
    - Diffuse black, `0.2`, `0.5`, and `1.0` albedo probes (`roughness 0.0`, `metallic 0.0`, `ior 1.0`).
    - White dielectric probes at roughness `0.05`, `0.25`, `0.5`, `1.0` (`metallic 0.0`, `ior 1.5`).
    - White metallic probes at roughness `0.05`, `0.25`, `0.5`, `1.0` (`metallic 1.0`, `ior 1.5`).
  - All cases load the white environment and show it as the visible background, while disabling uniform ambient light, direct lighting, SSAO, SSR, shadows, transparency, and tone mapping.
  - Deferred explicitly uses full IBL settings: specular IBL enabled, intensity `1.0`, and no roughness cutoff.

## Numeric Furnace Assertions

- Extend the render-test manifest with optional `material` and `white_furnace` objects:
  - `material`: inline overrides for `name`, `albedo`/`color`, `emission`, `roughness`, `metallic`, `reflectance`, `ior`, and additional PBR lobes.
  - `max_channel`: global upper bound for the HDR image (`1.01`–`1.05` for `PathTracer`, `1.05` for `DeferredRenderer`).
  - `min_channel`: global lower bound across the image, tightened per preset (`0.95` for white `PathTracer` probes, `0.80`–`0.85` for white `DeferredRenderer` probes) so silhouette/rim darkening is caught.
  - Two named normalized probe rectangles per test case:
    - `<preset>` (`[0.40, 0.40, 0.20, 0.20]` at the sphere center, normal incidence).
    - `<preset>_rim` (`[0.14, 0.45, 0.08, 0.10]` near the sphere's grazing rim).
- Add white-furnace and material-override data types to `RenderTestCase`, parser validation, and a CPU evaluator:
  - Validate finite, non-negative expected colors and tolerances.
  - Validate normalized rectangles lie inside the rendered image.
  - Compute each probe's mean RGB/luminance and the global HDR channel range.
  - Fail the explicitly run case when a probe target or global energy bound is violated.
- Define center and rim patch checks for each preset:
  - Diffuse black (`0.0`), `0.2`, `0.5`, and `1.0` probes target their respective linear albedos (`0.005`–`0.02` tolerance for `PathTracer`, `0.02`–`0.10` for `DeferredRenderer`).
  - Dielectric (`r005..r100`) and metallic (`r005..r100`) white probes explicitly target `[1.0, 1.0, 1.0]` at both center and rim (`0.02` tolerance for `PathTracer`, `0.10` center / `0.15` rim for `DeferredRenderer`).
- Always write `actual.pfm`, preview PNG, and `furnace_metrics.txt` for these cases, on both pass and failure. Metrics list the probe's measured RGB/luminance and the exact failed bound.

## Runner and Documentation

- Keep the furnace manifest out of normal `--all` and CTest regression labels so known renderer deviations do not block ordinary visual regression work.
- Add an explicit CMake target, `RenderWhiteFurnace`, which runs:

```powershell
RenderRegression --manifest Tests/RenderTests.WhiteFurnace.json --all
```

- Update `Docs/RENDER_TEST_FRAMEWORK_GUIDE.md` with the command, expected artifacts, interpretation of failures, and the rule that the suite diagnoses energy behavior but does not imply an automatic renderer correction.
- Add parser and evaluator unit tests for valid furnace declarations, invalid regions/values, diffuse target mismatch, negative output, and over-unity output.

## Verification

- Build `RenderRegression` and `RenderLab` in Debug.
- Run `RenderRegression --unit`.
- Run each furnace case individually, then the dedicated manifest through `RenderWhiteFurnace`.
- Confirm Test5 lists `WhiteFurnace.scene` and that both deferred and path-traced views show the same calibrated grid.
- Preserve all observed failures as diagnostics; do not alter deferred or path-tracing lighting, BRDF, environment sampling, or post-processing code as part of this work.

## Assumptions

- The test targets only deferred and path-traced rendering; software rasterizer coverage is out of scope.
- The white environment is linear radiance `1.0`, not display-white after tone mapping.
- The separate suite may initially fail, and that failure is the intended measurement baseline for later renderer-improvement work.
