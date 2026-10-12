# White Furnace Energy-Conservation Test Plan

## Summary

Add an explicit, non-default white-furnace suite for the deferred renderer and path tracer. It will render a calibrated sphere grid under a uniform linear-white environment, measure HDR output numerically, and report energy violations without changing any renderer or shader behavior.

## Implementation Changes

- Add a committed `64x32` linear Radiance HDR asset containing constant `(1,1,1)` radiance. Use the existing environment-map loader; no production environment API changes are needed.
- Add `Assets/WhiteFurnace.scene`, auto-discovered by Test5:
  - Orthographic `4x3` sphere grid, visible white environment background (physical furnace walls), no local lights.
  - Row 1: diffuse black, `0.2`, `0.5`, and `1.0` albedo probes.
  - Row 2: white dielectric probes at roughness `0.05`, `0.25`, `0.5`, `1.0`.
  - Row 3: white metallic probes at the same roughness values.
  - No textures, emission, transmission, clearcoat, or anisotropy.
- Add `Tests/RenderTests.WhiteFurnace.json`, separate from `RenderTests.json`:
  - `deferred_white_furnace`: one frame at `512x384`.
  - `pathtracer_white_furnace`: `256` accumulated frames at `512x384`, one sample per pixel, two bounces, no denoising, auto-scaling, or tile rendering.
  - Both cases load the white environment and show it as the visible background, while disabling uniform ambient light, direct lighting, SSAO, SSR, shadows, transparency, and tone mapping.
  - Deferred explicitly uses full IBL settings: specular IBL enabled, intensity `1.0`, and no roughness cutoff.

## Numeric Furnace Assertions

- Extend the render-test manifest with an optional `white_furnace` object:
  - `max_channel`: global upper bound for the HDR image, set to `1.05`.
  - `min_channel`: lower bound, set to `-0.001`.
  - Named normalized probe rectangles with optional expected RGB values and absolute tolerances.
- Add white-furnace data types to `RenderTestCase`, parser validation, and a CPU evaluator:
  - Validate finite, non-negative expected colors and tolerances.
  - Validate normalized rectangles lie inside the rendered image.
  - Compute each probe's mean RGB/luminance and the global HDR channel range.
  - Fail the explicitly run case when a diffuse reference target or energy bound is violated.
- Define fixed center patches for all twelve spheres:
  - Diffuse black must remain near `0`.
  - Diffuse `0.2`, `0.5`, and `1.0` probes target their respective linear albedos with documented tolerances.
  - Dielectric and metallic probes are coverage/energy probes: they must remain non-negative and not exceed the global white-furnace limit.
- Always write `actual.pfm`, preview PNG, and `furnace_metrics.txt` for these cases, on both pass and failure. Metrics list each probe's measured RGB/luminance and the exact failed bound.

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
