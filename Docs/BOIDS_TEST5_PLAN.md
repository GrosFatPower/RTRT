# Boids Simulation In Test5

## Summary

Status: implemented in the current codebase.

`Test5` now includes an optional CPU Boids simulation as an appended dynamic overlay on the currently loaded scene. Boids use a procedural low-poly arrow mesh so movement direction is visible in all three renderers. The implementation is split into reusable simulation and scene-binding pieces, with `Test5` owning UI, lifecycle, and per-frame orchestration.

## Key Changes

- Added a reusable `BoidSimulation` module with no renderer dependency:
  - `BoidSettings`: count, seed, bounds center/radius/height, min/max speed, max force, neighbor radius, separation radius, weights, boid scale, pause flag.
  - `BoidState`: position, velocity.
  - `Initialize(settings)`, `Reset(settings)`, `Resize(settings)`, `Update(deltaTime, settings)`, `GetBoids()`.
  - Use simple O(N^2) neighbor search for v1, targeting 32-256 boids.
- Added a reusable `BoidSceneBinding` helper that depends on `Scene` but not `Test5`:
  - Creates one procedural arrow `Mesh`, one matte material, and N appended `MeshInstance`s.
  - Tracks boid-owned instance IDs.
  - `Attach(Scene&, settings)`, `Detach(Scene&)`, `SyncTransforms(Scene&, simulation, settings)`, `ContainsInstanceID(int)`.
  - Removes only boid-owned mesh instances on disable/reset/scene reload; leaves the procedural mesh/material in the scene because `Scene` has no remove APIs for those.
- Integrated into `Test5`:
  - Added `_BoidsEnabled`, `_BoidsSettings`, `_BoidsSimulation`, `_BoidsBinding`, and a `DrawBoidsUI()` panel.
  - On scene load, detach/reset boid state; if the flag is enabled, attach boids to the new scene.
  - In `UpdateScene()`, after scene/renderer reload handling and before renderer `Update()`, advance simulation when enabled and not paused, sync transforms, then call `_Renderer->Notify(DirtyState::SceneInstances)`.
  - On disable, detach boid instances and notify `SceneInstances`.
  - If boid count changes, resize simulation and binding, then notify `SceneInstances`.

## Behavior Details

- Default settings:
  - enabled: off
  - count: 128
  - seed: 1337
  - bounds: centered around the current scene AABB if finite, otherwise origin; radius 4.0, height 3.0
  - speed: min 0.5, max 2.5
  - max force: 4.0
  - neighbor radius: 1.2
  - separation radius: 0.35
  - weights: separation 1.6, alignment 1.0, cohesion 1.0, bounds 2.0
  - scale: 0.12
- Boid mesh:
  - Procedural arrow/wedge mesh modeled along local +Z.
  - Transform basis is built from velocity direction, using world up unless nearly parallel.
  - Material uses opaque colored albedo, roughness 0.55, metallic 0.
- UI:
  - A `Boids` collapsing header exists in the existing `Test5` panel.
  - Controls: enable, pause, reset, recompute bounds, count, seed, color, speed range, neighbor/separation radii, rule weights, bounds radius/height, scale.
  - Changes that alter transforms notify `SceneInstances`; changes that recreate instances also update binding before notifying.
- Renderer expectations:
  - Path tracer will reset accumulation every moving frame because `SceneInstances` dirties TLAS data. This is expected and useful for the dynamic-geometry test.
  - Path tracer denoising and pass timers are now guarded so continuously dirty boid frames do not wait on denoiser GPU queries that were not issued.
  - Deferred renderer will rebuild draw lists/bounds and render moving instances.
  - Software rasterizer will reload compiled scene buffers when instances move; keep default count modest.

## Test Plan

- Build: `cmake --build build --config Debug --target RenderLab`.
- Runtime smoke test in `Test5`:
  - Enable boids on a normal loaded scene and confirm visible moving arrow meshes.
  - Toggle PathTracer, SoftwareRasterizer, and OpenGLRasterizer; confirm boids move in all three.
  - Change count, reset seed, pause/resume, disable/re-enable; confirm no crash and no stale visible boids.
  - Reload scene while boids are enabled; confirm boids attach to the new scene and renderer reload still succeeds.
  - Disable boids; confirm boid instances disappear and normal mesh-instance selection/editing still works.

## Assumptions

- V1 is CPU-only and intentionally simple; spatial acceleration can be added later behind `BoidSimulation` without changing `Test5`.
- Boids are an appended overlay, not a replacement scene.
- Procedural mesh/material are acceptable and preferred over new asset files.
- No renderer-specific boids code should be added; renderers should see ordinary moving `MeshInstance`s through existing dirty-state paths.
