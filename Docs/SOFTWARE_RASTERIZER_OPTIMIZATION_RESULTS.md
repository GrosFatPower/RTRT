# Software Rasterizer Optimization Results

## Reference Setup

- Machine: Apple M4, macOS
- Scene snapshot: `BenchmarkResults/Software_M4_BenchmarkScene.fpsmap`
- Benchmark v2 baseline: `BenchmarkResults/Software_M4_BenchmarkV2_Baseline.json`
- Final retained configuration: `BenchmarkResults/Software_M4_BenchmarkV2_Final.json`
- Resolution: 2560 x 1440
- Renderer: Software, PBR, W-buffer, tiled, 10 threads
- Samples: three repetitions, 10 warm-up frames, 120 measured frames per repetition

CPU frame time is the canonical total. `Render scene` is inclusive and must not be added to its child passes.

## Final Configuration

- Scalar rendering with 64-pixel tiles on Apple M4.
- Incremental instance transform refresh.
- Compact tiled hit records with generation stamps.
- Direct main-buffer color writes and uncovered-only background rendering.
- Conservative instance frustum culling with vertex-balanced worker batches.
- Double-buffered PBO color upload on Apple; direct texture upload elsewhere.

The fixed-pose median improved from 55.22 ms to 49.62 ms, a 10.13 percent reduction. Renderer update improved from 51.51 ms to 40.73 ms. The final configuration passes the software image-regression suite.

## Clean Transfer Validation

The reconstructed branch was benchmarked again after rejected experiments and their interfaces were removed:

| Pose | Median CPU frame | Render scene median | CPU upload median |
| --- | ---: | ---: | ---: |
| Fixed | 49.24 ms | 35.48 ms | 0.36 ms |
| Ground | 49.98 ms | 42.34 ms | 0.38 ms |
| Sky | 25.81 ms | 11.88 ms | 0.37 ms |

The cleaned fixed result is within 0.8 percent of the retained experimental result and 10.8 percent faster than the Benchmark v2 baseline. Release and Debug macOS builds and all five software regression cases passed.

## Retained Results

| Optimization | Result | Decision |
| --- | --- | --- |
| Incremental refresh | Transform refresh 0.948 ms to 0.037 ms, about 96% faster | retained |
| Compact hits | Rasterization about 14.5% faster; measured hit storage 280.6 MB to 118.0 MB | retained |
| Direct color writes | Fixed -5.64%; ground -22.15%; sky -8.26% CPU median | retained |
| Batched frustum culling | Vertex processing -66% to -83%; render scene up to -4.00% | retained |
| Apple PBO upload | CPU upload about 1.64 ms to 0.37 ms; whole-frame gain varied from 0.5% to 6.5% | retained on Apple |

## Rejected Experiments

The following implementations are intentionally absent from the production transfer:

- ARM NEON as the M4 default: about 5.1% slower than scalar.
- Tile sizes 32, 128, and 256: all slower than 64 on M4.
- Generic `ParallelFor` renderer scheduling: total frame time regressed by 0.36%.
- Shared/stack fragment shader dispatch: removed allocations but total time regressed by 0.58%.
- Cached material bindings: fragment and total frame time regressed.
- Compact-hit pointer clearing: rasterization improved but clearing caused a larger total regression.
- One-job-per-instance frustum dispatch: vertex processing regressed by about 390%.

The full timestamped benchmark history remains on `codex/software-rasterizer-optimizations`.

## Benchmarking

Test6 can benchmark the active renderer from its Benchmark panel. Software automation also supports cumulative presets:

```sh
./build/Release/RenderLab Test6 --benchmark-software LABEL scalar 64 PRESET fixed
```

Valid presets are `none`, `incremental`, `compact-hits`, `direct-color`, `frustum-culling`, and `pbo-upload`. Valid poses are `fixed`, `ground`, and `sky`.

Compare two Benchmark v2 results with:

```sh
Tools/compare_benchmarks.py BEFORE.json AFTER.json
```
