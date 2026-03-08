# Shadow Mapping Implementation Recap

Date: 2026-03-08
Feature: v1 local-light shadow mapping for the deferred `OpenGLRasterizer`

## Goal

Implemented omnidirectional shadow mapping for one local light in the deferred OpenGL renderer, prioritizing `SphereLight` and falling back to `RectLight`.

The selected shadow-casting light is:
- the first `SphereLight` found in scene order, or the first `RectLight` if no sphere light exists
- treated as a point-light shadow source located at `Light._Pos`
- used only for direct-light shadowing in the deferred lighting pass

## Main Code Changes

### Renderer state and resources

Updated `Source/src/DeferredRenderer.h`:
- added a shadow cubemap texture slot
- added a new deferred debug flag for shadow visualization
- added shadow framebuffer and depth cubemap members
- added cached shadow-light state
- added six shadow view-projection matrices
- added shadow-related methods:
  - `InitializeShadowMap()`
  - `UpdateShadowState()`
  - `RenderShadowMap()`
  - `ComputeSceneBounds()`
  - `ComputeAutoShadowFar()`

Updated `Source/src/DeferredRenderer.cpp`:
- initialize and destroy shadow resources
- detect the first sphere light and cache its position/index, with fallback to the first rect light
- compute scene bounds from mesh-instance bounding boxes
- auto-fit shadow far plane when `_ShadowFar <= 0`
- build six cubemap face transforms for the selected local light using `Light._Pos` as the cubemap origin
- allocate and configure the depth cubemap and shadow framebuffer
- add a shadow render pass before the G-buffer lighting pass
- bind the shadow cubemap during deferred lighting
- send shadow uniforms to the lighting shader

### Render settings and UI

Updated `Source/src/RenderSettings.h` with new deferred-renderer settings:
- `_ShadowMapping`
- `_ShowShadowMap`
- `_ShadowMapResolution`
- `_ShadowBias`
- `_ShadowFar`

Updated `Source/src/Test5.cpp` under `OpenGLRasterizer` settings:
- added `Shadow mapping` checkbox
- added `Shadow map resolution` slider
- added `Shadow bias` slider
- added `Shadow far plane` slider
- added `Show shadow factor` checkbox
- wired the shadow debug toggle into deferred debug mode selection

### Shader changes

Added `Shaders/vertex_ShadowCubeDepth.glsl`:
- transforms mesh vertices into the current shadow cubemap face clip space
- forwards world position for depth computation

Added `Shaders/fragment_ShadowCubeDepth.glsl`:
- computes normalized radial distance from light to fragment
- writes that value into depth

Updated `Shaders/fragment_DeferredLighting.glsl`:
- added `samplerCube u_ShadowMap`
- added shadow uniforms:
  - `u_EnableShadowMapping`
  - `u_ShadowLightIndex`
  - `u_ShadowLightPos`
  - `u_ShadowBias`
  - `u_ShadowFar`
- added `ComputeShadow()` using cubemap sampling and a simple PCF-style kernel
- applied the visibility factor only to the selected shadow-casting sphere or rect light
- added a shadow-factor debug visualization path

## Behavior After The Change

- If at least one `SphereLight` exists and `_ShadowMapping` is enabled, the renderer now builds a shadow cubemap and uses it during deferred lighting.
- The first `SphereLight` casts shadows in v1, or the first `RectLight` if no sphere light exists.
- `RectLight` is now supported as a fallback shadow caster using the quad center at `Light._Pos`; `DistantLight` remains unshadowed.
- Ambient/background/env-map contribution remains unshadowed.
- If no `SphereLight` exists but a `RectLight` does, the deferred renderer uses the rect-light center as the shadow emitter. If neither exists, it falls back to the previous unshadowed behavior.

## Build Verification

Build verification performed with:
- `cmake --build Build --config Debug --target RT_renderer`

Result:
- build succeeded

Observed warning:
- linker warning `LNK4098` about `LIBCMT` conflict was present during the build output
- this warning was not introduced or addressed as part of the shadow-mapping feature

## Files Changed

Modified:
- `Source/src/DeferredRenderer.h`
- `Source/src/DeferredRenderer.cpp`
- `Source/src/RenderSettings.h`
- `Source/src/Test5.cpp`
- `Shaders/fragment_DeferredLighting.glsl`

Added:
- `Shaders/vertex_ShadowCubeDepth.glsl`
- `Shaders/fragment_ShadowCubeDepth.glsl`

## Known Limitations Of This V1

- only one local light casts shadows at a time
- sphere radius does not produce soft area-light shadows
- primitive instances are not added to the shadow pass
- no cube-map arrays or multi-light shadowing
- no VSM/ESM/PCSS or temporal stabilization
- shadow debug view shows shadow factor, not the six cubemap faces directly

## Suggested First Visual Checks

- test an interior scene with a clear sphere light or rect light and nearby occluders
- tune `_ShadowBias` to reduce acne and peter-panning
- verify whether `_ShadowFar` is too large for small scenes
- check that cubemap face seams are not obvious in normal camera views

