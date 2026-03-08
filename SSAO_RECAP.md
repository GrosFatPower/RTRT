# SSAO Implementation Recap

Date: 2026-03-08
Feature: polished v1 SSAO for the deferred `OpenGLRasterizer`

## Goal

Implemented screen-space ambient occlusion for the deferred OpenGL renderer, with:
- a dedicated SSAO fullscreen pass
- a lightweight blur pass
- UI controls in `Test5`
- a deferred `SSAO` debug buffer view

The effect uses the existing deferred G-buffer world positions, normals, and depth to add contact darkening in creases and near geometry intersections.

## Main Code Changes

### Renderer state and resources

Updated `Source/src/DeferredRenderer.h`:
- added deferred texture slots for:
  - SSAO output
  - blurred SSAO output
  - SSAO noise texture
- added a deferred debug mode for SSAO visualization
- added SSAO framebuffer and texture members
- added SSAO shader handles
- added a cached SSAO kernel
- added SSAO-related methods:
  - `InitializeSSAO()`
  - `BindSSAOPassTextures()`
  - `RenderSSAO()`

Updated `Source/src/DeferredRenderer.cpp`:
- initialize and destroy SSAO resources
- generate a fixed hemisphere sample kernel on the CPU
- generate a small tiled noise texture for sample rotation
- add an SSAO pass after the G-buffer pass and before deferred lighting
- add a blur pass to stabilize the raw AO result
- resize SSAO targets when the render resolution changes
- bind the blurred AO texture during deferred lighting
- send SSAO uniforms to the AO, blur, and lighting shaders

### Render settings and UI

Updated `Source/src/RenderSettings.h` with new deferred-renderer settings:
- `_SSAO`
- `_SSAOBlur`
- `_SSAORadius`
- `_SSAOBias`
- `_SSAOIntensity`
- `_SSAOKernelSize`

Updated `Source/src/Test5.cpp` under `OpenGLRasterizer` settings:
- added `SSAO` checkbox
- added `SSAO blur` checkbox
- added `SSAO radius` slider
- added `SSAO bias` slider
- added `SSAO intensity` slider
- added `SSAO kernel size` slider
- extended the deferred `Buffer` combo to include `SSAO`
- wired the new buffer entry into deferred debug mode selection

### Shader changes

Added `Shaders/fragment_SSAO.glsl`:
- samples G-buffer world position, normal, and depth
- transforms samples into view space for screen-space AO comparison
- uses a small noise texture to rotate the sample basis per pixel
- outputs a raw AO factor in grayscale

Added `Shaders/fragment_SSAOBlur.glsl`:
- blurs the raw AO result with a lightweight edge-aware 3x3 filter
- uses depth and normal similarity to avoid obvious bleeding across edges
- supports blur bypass when `_SSAOBlur` is disabled

Updated `Shaders/fragment_DeferredLighting.glsl`:
- added `sampler2D u_SSAOMap`
- added SSAO uniforms:
  - `u_EnableSSAO`
  - `u_SSAOIntensity`
- samples the blurred AO result during deferred lighting
- applies AO to the ambient term and lightly modulates direct lighting
- added an SSAO debug visualization path

Updated `Shaders/fragment_DeferredGeometry.glsl`:
- fixed hit-point position and distance setup to use `fragWorldPos` directly instead of reading `gPosition` before it was written

## Behavior After The Change

- In `OpenGLRasterizer`, SSAO now runs after the geometry pass and before deferred lighting.
- If `_SSAO` is enabled, nearby geometry now adds ambient/contact darkening in corners and contact areas.
- If `_SSAOBlur` is enabled, the raw AO result is smoothed before it reaches the lighting pass.
- The deferred `Buffer` combo now contains an `SSAO` entry that shows the AO factor directly.
- Background and sky pixels remain unaffected and resolve to white in the AO buffer.
- Path tracing and software rasterization remain unchanged.

## Build Verification

Build verification performed with:
- `cmake --build Build --config Debug --target RT_renderer`

Result:
- build succeeded

Observed warning:
- linker warning `LNK4098` about `LIBCMT` conflict was present during the build output
- this warning was not introduced or addressed as part of the SSAO feature

## Files Changed

Modified:
- `Source/src/DeferredRenderer.h`
- `Source/src/DeferredRenderer.cpp`
- `Source/src/RenderSettings.h`
- `Source/src/Test5.cpp`
- `Shaders/fragment_DeferredGeometry.glsl`
- `Shaders/fragment_DeferredLighting.glsl`

Added:
- `Shaders/fragment_SSAO.glsl`
- `Shaders/fragment_SSAOBlur.glsl`

## Known Limitations Of This V1

- SSAO is full-resolution only and does not use half-resolution optimization
- no temporal accumulation or history stabilization
- no bent normals or multi-bounce AO approximation
- sample kernel remains fixed after initialization
- AO is an image-space approximation and may halo or under-occlude in some configurations
- AO currently affects ambient plus a light modulation term, not a physically based indirect-light model

## Suggested First Visual Checks

- test contact darkening under small props resting on larger surfaces
- inspect room corners and tight mesh intersections for improved grounding
- switch `Buffer` to `SSAO` and confirm background pixels stay white
- compare blur on versus off for noise versus edge sharpness
- tune `_SSAORadius`, `_SSAOBias`, and `_SSAOIntensity` on both small and large scenes
- confirm SSAO and shadow mapping work together without obvious conflicts
