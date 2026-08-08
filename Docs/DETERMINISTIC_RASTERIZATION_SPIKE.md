# Deterministic Rasterization Spike

## Goal

Evaluate whether fixed-point scalar triangle coverage and a top-left fill rule reduce the macOS Software renderer differences against the existing Windows baselines.

The spike is contained on the `deterministic-rasterization-spike` branch. It does not update baselines or alter the tolerant Software profile in `Tests/RenderTests.json`.

## Implementation

- Screen-space coverage uses a 1/256-pixel fixed-point grid and 64-bit edge equations.
- Pixels on shared edges are assigned by a y-up top-left rule.
- Float barycentrics, depth interpolation, fragment shading, texture sampling, clipping, and SIMD rasterization remain unchanged.
- The scalar tiled path uses the same coverage bounds for tile binning and pixel traversal.
- `Tests/RenderTests.DeterministicRasterization.json` runs the four Software scenes in non-tiled and tiled modes against the existing Windows baselines with the original strict thresholds.

## macOS Results

Artifacts were captured from `build/Debug` with the strict manifest before and after the coverage change:

- Baseline: `Tests/Artifacts/deterministic_coverage/baseline`
- Candidate: `Tests/Artifacts/deterministic_coverage/candidate`

| Case | Mode | MAE reduction | Mismatch reduction | Result |
| --- | --- | ---: | ---: | --- |
| Textured Box | Non-tiled | 98.91% | 55.56% | Missed mismatch gate |
| Cornell | Non-tiled | Regressed | Regressed | Failed |
| Dining Room | Non-tiled | 63.78% | 4.96% | Failed |
| Teapot Env | Non-tiled | 95.30% | 60.61% | Missed mismatch gate |
| Textured Box | Tiled | 97.03% | 31.03% | Missed mismatch gate |
| Cornell | Tiled | Regressed | Regressed | Failed |
| Dining Room | Tiled | 62.36% | 4.94% | Failed |
| Teapot Env | Tiled | 95.30% | 60.61% | Missed mismatch gate |

The target required at least 80% lower MAE and mismatch count for every case and mode. The spike does not meet that target.

Candidate diff images show residual differences concentrated on scene geometry edges. No obvious holes, cracks, or tile-grid seams appeared in the reviewed Dining Room and Cornell diagnostics.

## Conclusion

Fixed-point coverage improves some aggregate color error, especially Textured Box and Teapot Env, but it does not remove the dominant Dining Room mismatch count and it regresses Cornell. The remaining differences are therefore likely to include float interpolation, depth ordering, texture sampling, or compiler floating-point behavior.

Keep this branch for review only. Do not merge the experiment as a deterministic-rasterization solution without a follow-up investigation of those remaining stages.
