# Render Test Framework Guide

## Purpose

`RenderRegression` renders fixed scenes through the software rasterizer, deferred renderer, or path tracer, then compares the linear final image against a committed PFM baseline. It is intended to detect visual regressions from rendering, shader, material, camera, and lighting changes.

Render cases are declared in `Tests/RenderTests.json`. CMake reads this manifest and creates one CTest entry per case.

## Build

From the repository root, build the test runner in Debug:

```powershell
cmake --build Build --config Debug --target RenderRegression
```

The build reconfigures automatically when `Tests/RenderTests.json` changes, so CTest discovers added, removed, or renamed cases.

## Run Tests

List all render cases:

```powershell
.\Build\Debug\RenderRegression.exe --list
```

Run the framework unit tests only:

```powershell
.\Build\Debug\RenderRegression.exe --unit
ctest --test-dir Build -C Debug -L unit --output-on-failure
```

The runner reports the individual unit groups before its unit summary. Current coverage includes PFM I/O, image comparison, diagnostic-image creation, valid manifest parsing, and invalid manifest validation.

Run one render case:

```powershell
.\Build\Debug\RenderRegression.exe --case deferred_teapot_env
```

Run every render case through CTest:

```powershell
ctest --test-dir Build -C Debug -L render --output-on-failure
```

Filter CTest by backend when narrowing a failure:

```powershell
ctest --test-dir Build -C Debug -L deferred --output-on-failure
ctest --test-dir Build -C Debug -L software --output-on-failure
ctest --test-dir Build -C Debug -L pathtracer --output-on-failure
```

Run unit and render cases directly through the runner:

```powershell
.\Build\Debug\RenderRegression.exe --all
```

The runner prints one compact status line per render case and a final summary:

```text
[PASS] deferred_teapot_env (2.41 s)
[FAIL] deferred_example (1.02 s) - image mismatch; see Tests/Artifacts/deferred_example
Summary: 12 passed, 1 failed, 0 skipped, 0 updated.
```

Interactive Windows consoles use color for pass, failure, skip, and baseline-update states. Color is disabled automatically when output is redirected or captured by CTest.

The runner creates a hidden OpenGL context. A machine without a compatible OpenGL context reports CTest skip code `77`; image mismatches and rendering failures remain failures.

## Review Failures

Regular runs never modify baselines. A failed render writes diagnostics to `Tests/Artifacts/<case-name>/`:

- `trace.log`: loader, renderer, shader, and runner diagnostics captured for this case.
- `actual.pfm`: newly rendered linear image.
- `expected.pfm`: committed baseline copied for comparison.
- `actual.png` and `expected.png`: tone-mapped images for quick inspection.
- `diff.png`: amplified visual difference.
- `metrics.txt`: mean error, maximum error, mismatch count, and mismatch ratio.

Use `--artifacts` to keep an experiment separate from the default artifacts directory:

```powershell
.\Build\Debug\RenderRegression.exe --case deferred_teapot_env --artifacts Tests\Artifacts\teapot-investigation
```

Every render case writes a trace log, including passing cases. Inspect it when a concise status line needs more context:

```powershell
Get-Content Tests\Artifacts\deferred_teapot_env\trace.log
```

PFM files are linear floating-point images and may appear vertically flipped in a generic viewer. Prefer the generated PNG diagnostics for visual review.

## Rebaseline

Rebaseline only after reviewing the intended visual change. Update one baseline:

```powershell
.\Build\Debug\RenderRegression.exe --case deferred_teapot_env --update-baselines
```

Update all baselines:

```powershell
.\Build\Debug\RenderRegression.exe --all --update-baselines
```

Then inspect the resulting PNG output, run the relevant comparison again, and stage only the approved PFM files under `Tests/Baselines/`.

## Manifest

`Tests/RenderTests.json` has a schema version, reusable profiles, and test cases. A profile supplies the renderer backend, default resolution, frame count, thresholds, and fixed backend settings. A test selects one profile and overrides only what differs.

```json
{
  "name": "deferred_example",
  "profile": "deferred",
  "scene": "example.scene",
  "environment_map": "HDR/example.hdr",
  "frames": 5,
  "camera": {
    "position": [0.0, 1.0, 4.0],
    "pivot": [0.0, 1.0, 0.0],
    "fov": 45.0,
    "near": 0.5
  }
}
```

Required test fields are `name`, `profile`, and `scene`. Names must be unique and profiles must exist. Omitting `baseline` uses `Tests/Baselines/<name>.pfm`.

Supported optional test fields are `resolution`, `frames`, `thresholds`, `environment_map`, `camera`, `baseline`, `debug_mode`, and `diagnostic_only`. Resolution values and frame counts must be positive. A camera requires three-number `position` and `pivot` arrays plus a positive `fov`; `near` is positive and `far` must be greater than `near`.

`debug_mode` is a non-negative renderer debug-mode bit mask. Set `diagnostic_only` to `true` with a debug mode to save `actual.pfm` and `actual.png` without reading, comparing, or modifying a baseline. The runner reports these cases as `CAPTURED`; use the same diagnostic manifest on two platforms and compare the generated artifacts when isolating a renderer difference.

Use a separate manifest while experimenting, without editing the committed catalog:

```powershell
.\Build\Debug\RenderRegression.exe --manifest Tests\RenderTests.local.json --list
.\Build\Debug\RenderRegression.exe --manifest Tests\RenderTests.local.json --case deferred_example
```

The runner validates manifest fields before creating its OpenGL context and reports actionable parse errors. CTest entries are generated only from the committed `Tests/RenderTests.json`; a custom manifest is intended for local runner experiments.

## Adding a Case

1. Choose an existing profile, or add a new one when its defaults are genuinely reusable.
2. Add a uniquely named test with its scene and necessary overrides.
3. Build `RenderRegression` so CMake reconfigures and registers the new CTest entry.
4. Render it with `--update-baselines`.
5. Review the generated image and commit the approved baseline and manifest.

Example:

```powershell
.\Build\Debug\RenderRegression.exe --case deferred_example --update-baselines
ctest --test-dir Build -C Debug -R Render_deferred_example --output-on-failure
```

## Determinism Notes

- Software tests use very strict tolerances and should be stable.
- Deferred and path-traced tests use calibrated tolerances because driver, floating-point, and sampling differences can affect pixels.
- Path-tracer cases must specify enough frames for a useful accumulated image. Keep tiled rendering disabled for reference captures unless a test explicitly targets tiles.
- A baseline is hardware and driver sensitive. Review it on the reference configuration before accepting broad tolerance changes.
