# Distant Light Shadow Priority Recap

Date: 2026-03-08
Feature: `DistantLight` now takes priority as the deferred shadow emitter

## Current Status Note

This recap still matches the current shadow-emitter priority in code.

The current CMake executable target name is `RenderLab`.

## Goal

When a scene contains a `DistantLight`, use it as the shadow emitter in the deferred `OpenGLRasterizer` instead of falling back to sphere or rect lights.

## What Changed

Updated `Source/src/DeferredRenderer.h`:
- split the shadow resources into a cubemap path and a 2D depth-map path
- added shadow-emitter type tracking with `LightType`
- added directional shadow state including light direction and a directional light view-projection matrix
- added separate shadow shader handles for cube and directional passes

Updated `Source/src/DeferredRenderer.cpp`:
- shadow selection priority is now:
  - first `DistantLight`
  - otherwise first `SphereLight`
  - otherwise first `RectLight`
- when a distant light is selected, the renderer now:
  - interprets `Light._Pos` as the distant-light direction, consistent with the lighting shader
  - builds a directional light-space view from the scene bounds
  - fits an orthographic shadow projection around the scene extent
  - renders a 2D depth shadow map instead of a depth cubemap
- retained the existing cubemap shadow path for sphere and rect fallback lights
- updated uniform binding so the lighting pass knows whether it should sample the cube map or the 2D map

Updated `Shaders/fragment_DeferredLighting.glsl`:
- split shadow sampling into two paths:
  - local-light cubemap sampling for sphere/rect emitters
  - orthographic 2D shadow-map sampling for distant lights
- added uniforms for:
  - `u_ShadowCubeMap`
  - `u_Shadow2DMap`
  - `u_ShadowLightType`
  - `u_ShadowLightDir`
  - `u_ShadowLightViewProj`
- kept the debug shadow-factor view working with both shadow types

Added new directional shadow shaders:
- `Shaders/vertex_ShadowDirectionalDepth.glsl`
- `Shaders/fragment_ShadowDirectionalDepth.glsl`

## Behavior After The Change

- If a scene contains at least one `DistantLight`, the first distant light now becomes the shadow emitter.
- If there is no distant light, the renderer still falls back to the first sphere light.
- If there is no sphere light either, it still falls back to the first rect light using the quad center at `Light._Pos`.
- Directional shadows now use an orthographic depth map instead of the local-light cubemap path.

## Verification

Build verification at the time used the then-current target naming.

Current equivalent local build command:
- `cmake --build build --target RenderLab`

Result:
- build succeeded

## Files Changed

Modified:
- `Source/src/DeferredRenderer.h`
- `Source/src/DeferredRenderer.cpp`
- `Shaders/fragment_DeferredLighting.glsl`

Added:
- `Shaders/vertex_ShadowDirectionalDepth.glsl`
- `Shaders/fragment_ShadowDirectionalDepth.glsl`

## Suggested Visual Checks

- test a scene that contains both a `DistantLight` and local lights, and confirm the distant light wins consistently
- inspect shadow stability while orbiting the camera around a large scene
- check whether the directional shadow projection needs tighter fitting or extra bias tuning for grazing surfaces
