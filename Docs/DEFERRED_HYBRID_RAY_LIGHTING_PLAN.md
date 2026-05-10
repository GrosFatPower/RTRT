# Deferred Hybrid Ray-Lit Pass Feasibility Plan

## Summary
Yes, the hybrid approach is feasible, but it should not directly reuse `fragment_PathTracer.glsl` as-is. The clean path is to add a deferred fullscreen ray pass: rasterization provides the first visible hit through the G-buffer, then a GLSL ray-query pass reuses the existing BVH traversal, material loading, light sampling, BRDF, and environment sampling code to compute one secondary bounce or one stochastic indirect sample.

The main missing piece is shared GPU ray-scene resources. Today `PathTracer` owns the BVH/TBO upload path, while `DeferredRenderer` compiles mesh data without BVH. To keep this modular, factor that upload layer out before adding the hybrid pass.

## Key Changes
- Add a shared GPU ray-scene resource helper, for example `GLRayScene`, responsible for:
  - compiling mesh data with BVH enabled;
  - uploading vertices, normals, UVs, texture indices, materials, texture array, TLAS/BLAS nodes, transforms, and mesh/material IDs;
  - updating TLAS data when `SceneInstances` changes;
  - binding all ray-tracing textures to caller-provided texture slots.
- Migrate `PathTracer` to use this helper first, preserving behavior.
- Add the same helper to `DeferredRenderer`, enabled only when hybrid lighting is active, so normal deferred mode does not pay BVH upload cost unless needed.
- Extract reusable path-tracing shader logic into smaller includes:
  - ray scene binding/traversal: existing `RayTrace.glsl`, `BVH.glsl`, `Material.glsl`;
  - direct path-traced light evaluation: extracted from `fragment_PathTracer.glsl`;
  - environment sampling and MIS helpers: existing `Sampling.glsl`;
  - BRDF/Disney helpers: existing `DisneyBSDF.glsl` and/or current deferred `PBR.glsl`.
- Add a new deferred pass, for example `fragment_DeferredHybridGI.glsl`, that:
  - reads G-buffer position, normal, albedo/material/emission/depth;
  - reconstructs the first `HitPoint` from rasterized data;
  - samples one secondary ray from the surface BRDF;
  - traces that ray through the shared BVH;
  - evaluates direct lighting/environment at the second hit;
  - outputs an HDR indirect-light texture plus confidence/debug data.
- Add a new render target, for example `_HybridGITEX`, and blend it in `fragment_DeferredLighting.glsl` as an optional indirect term.

## Recommended V1 Behavior
- Add `RenderSettings` controls:
  - `_HybridRayLighting = false`
  - `_HybridRayLightingSamples = 1`
  - `_HybridRayLightingBounces = 1`
  - `_HybridRayLightingIntensity = 1.0f`
  - `_HybridRayLightingHalfRes = true`
  - `_HybridRayLightingTemporalAccumulation = false` for first drop unless explicitly enabled later.
- Start with one stochastic secondary bounce:
  - diffuse surfaces sample a cosine/Disney diffuse lobe;
  - glossy/metallic surfaces sample a specular/Disney reflection lobe;
  - misses sample the environment map;
  - second-hit lighting uses the path tracer's existing direct-light logic.
- Do not replace the current deferred direct lighting in v1. Treat hybrid output as optional indirect GI/reflection enrichment.
- Keep SSR and specular IBL as separate features for now. Later, hybrid rays can replace or augment SSR misses and rough specular IBL.
- Keep transparent objects out of the hybrid pass in v1, matching current deferred limitations.

## Pipeline Integration
- New deferred order:
  1. Shadow maps
  2. Opaque G-buffer
  3. SSAO
  4. SSR
  5. Hybrid ray lighting pass, if enabled
  6. Deferred lighting composition
  7. Transparent forward pass
  8. Debug/composite
- The hybrid pass should use the G-buffer first hit instead of tracing primary camera rays. This avoids duplicating raster work and keeps the first-hit result identical to deferred rendering.
- Use existing dirty states:
  - scene reload: rebuild full ray scene resources;
  - `SceneInstances`: rebuild/update TLAS only;
  - `SceneMaterials`/textures: update material and texture resources;
  - camera/settings: update uniforms only.
- Add debug views:
  - hybrid indirect color;
  - ray hit distance;
  - miss/confidence mask;
  - optional sample direction/normal debug.

## Main Risks
- Performance: GLSL BVH traversal in a fullscreen pass can be expensive, especially at full resolution. Defaulting to half-res is recommended.
- Noise: one stochastic sample per pixel will be noisy without temporal accumulation or denoising. V1 should be considered a debug/prototype path unless accumulation is added.
- Resource duplication: without a shared `GLRayScene`, `DeferredRenderer` and `PathTracer` will drift. Refactoring this first is the key cleanliness step.
- Shader coupling: `fragment_PathTracer.glsl` mixes camera generation, accumulation assumptions, debug paths, and path integration. Reuse should happen through extracted includes, not by calling the whole shader logic.

## Test Plan
- Build with `cmake --build build`.
- Validate GLSL with `glslangValidator` for the new includes and hybrid shader.
- Confirm `PathTracer` output remains unchanged after migrating to shared ray-scene resources.
- In deferred mode, test with hybrid lighting off and confirm no visual or performance regression.
- Enable hybrid lighting and verify:
  - second-bounce color appears on nearby diffuse surfaces;
  - glossy/metallic surfaces get plausible bounced/reflected contribution;
  - environment misses contribute correctly;
  - moving Test6 projectiles update TLAS correctly;
  - resize, render scale, debug views, SSR, SSAO, shadows, and render-to-file still work.
- Stress test with Test6, bedroom, and textured-box scenes at full-res and half-res.

## Assumptions
- The first implementation targets OpenGL 4.1/macOS compatibility, so this should be a fullscreen fragment pass rather than a compute shader.
- V1 is an experimental hybrid indirect-lighting path, not a replacement for the full path tracer.
- Direct deferred PBR lighting remains the default real-time lighting model.
- One-bounce hybrid lighting is enough for the first drop; temporal accumulation and denoising can be added after the core pass is stable.
