# Renderer Performance Suggestions

## Summary

This document collects candidate frame-time reductions for the Software Rasterizer and Deferred Renderer. It is advisory only: none of these items should be treated as implemented or as committed roadmap work until explicitly picked up.

The main theme is to measure first, then optimize the hottest pass for the active renderer, scene, and render settings.

## Shared Measurement Work

- Add or use a repeatable performance scene/mode for `Test5` and `Test6`.
- Use fixed camera poses, fixed scenes, fixed render scale, and fixed quality settings when comparing changes.
- Record median frame time over a short stable window instead of relying on one frame.
- Keep CPU renderer work separate from GPU copy/composite/present work in the UI.
- Use the existing renderer pass timings from `GetRenderPassTimings()` as the first source of truth.

## Software Rasterizer Suggestions

- Replace the per-frame `glTexImage2D` color-buffer upload with persistent texture allocation plus `glTexSubImage2D`; consider double-buffered PBO uploads if the copy-to-GPU path remains expensive.
- Reduce job-system overhead in non-tiled background rendering by submitting chunked row ranges instead of one job per row.
- Prefer tiled rendering as the primary path and tune default tile size per platform, likely starting with 64 or 128.
- Replace tile `std::vector<bool>` covered-pixel masks with a byte-sized mask such as `std::vector<uint8_t>`.
- Reduce per-frame fragment storage traffic by shading directly on depth wins in the tiled path, or by storing a smaller per-pixel hit record.
- Avoid allocating fragment shader objects inside each fragment-processing job; use stack objects or precomputed mode dispatch.
- Add coarse mesh-instance frustum culling before vertex processing and rasterization.
- Cache per-material texture availability and other repeated material lookups used during triangle setup and fragment shading.
- Enable SIMD automatically when supported, while keeping the UI toggle as an override.
- Cache per-instance transform and inverse-transpose matrices during dynamic instance refresh instead of recomputing them per source vertex.

## Deferred Renderer Suggestions

- Add half-resolution SSAO and SSR modes with depth/normal-aware upscaling.
- Skip the SSR source-copy blit when SSR is disabled and no SSR debug view is active.
- Move more pass-level early exits ahead of timing and setup work for disabled or empty passes, especially transparency, SSR, SSAO, shadows, and wireframe.
- Improve shadow-caster selection by ranking visible or high-contribution lights and keeping the active caster cap aggressive.
- Cache shadow maps when lights and shadow-casting scene instances are unchanged.
- Add lower-cost quality presets that adjust shadow resolution, max shadow casters, SSAO kernel size, SSR step count, SSR resolution, and render scale together.
- Review G-buffer bandwidth, especially whether position can be reconstructed from depth and whether normal/material targets can use tighter formats.
- Batch or instance repeated mesh draws where possible, especially Test6 boxes, projectiles, and repeated props.
- Avoid transparent index-buffer uploads when camera and transparent transforms are unchanged; consider dedicated dynamic EBOs for sorted transparent draws.
- Reduce SSR cost with early roughness/depth rejection, lower default step counts, and half-resolution rendering before considering larger hierarchical-Z work.

## Recommended Priority Order

1. Establish a repeatable measurement baseline for `Test5` and `Test6`.
2. Optimize the Software Rasterizer color-buffer upload and job granularity.
3. Add Deferred Renderer half-resolution SSAO/SSR and skip unnecessary SSR source copies.
4. Clean up Software Rasterizer tile masks and fragment storage.
5. Improve Deferred Renderer shadow caching and shadow-caster ranking.
6. Evaluate larger architecture work such as G-buffer compression, draw batching/instancing, and direct tile shading in the Software Rasterizer.
