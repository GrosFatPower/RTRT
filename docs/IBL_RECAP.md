# Specular IBL Implementation Recap

Date: 2026-03-16
Feature: specular image-based lighting (IBL) for the deferred `OpenGLRasterizer`

## Current Status Note

This recap still matches the current specular-IBL feature set in code.

The current CMake executable target name is `RenderLab`.

## Goal

Add specular environment reflections to the deferred OpenGL renderer using the existing lat-long env map with mipmap filtering and a preintegrated BRDF LUT.

## What Changed

### G-buffer additions

Updated the deferred G-buffer to include a material parameter target:
- Added `gMaterial` (RGB16F) storing roughness, metallic, and reflectance
- Updated the G-buffer FBO to include this new MRT attachment

Updated `Shaders/fragment_DeferredGeometry.glsl`:
- loaded material parameters
- wrote roughness, metallic, and reflectance into `gMaterial`

### BRDF LUT

Added a BRDF integration LUT pass:
- new shader `Shaders/fragment_BRDFLUT.glsl`
- new texture slot and framebuffer for the LUT
- generated once during renderer initialization with a fullscreen quad

### Env map filtering

Updated env map upload to support specular LOD sampling:
- enabled mipmaps on the env map texture
- set min filter to `GL_LINEAR_MIPMAP_LINEAR`

### Deferred lighting

Updated `Shaders/fragment_DeferredLighting.glsl`:
- sampled `gMaterial` for roughness/metallic/reflectance
- computed `F0 = mix(0.16 * reflectance^2, albedo, metallic)`
- computed a reflection vector and sampled the env map with `textureLod`
- sampled BRDF LUT for split-sum evaluation
- added the specular IBL term with intensity and light AO modulation

### Settings and UI

Updated `Source/src/RenderSettings.h`:
- added `_SpecularIBL` (default `true`)
- added `_SpecularIBLIntensity` (current default `0.5f`)

Updated `Source/src/Test5.cpp`:
- added `Specular IBL` checkbox
- added `IBL intensity` slider

## Behavior After The Change

- When environment mapping is enabled, specular IBL now adds reflections to the deferred renderer.
- Roughness drives the reflection blur via env map mip levels.
- Metallic materials show stronger, more colored reflections based on their albedo.
- The IBL term can be toggled and scaled through the new UI controls.

## Build Verification

Build verification at the time used the then-current target naming.

Current equivalent local build command:
- `cmake --build build --target RenderLab`

Result:
- build succeeded

Observed warning:
- linker warning `LNK4098` about `LIBCMT` conflict
- this warning was not introduced or addressed as part of the IBL feature

## Files Changed

Modified:
- `Source/src/RenderSettings.h`
- `Source/src/DeferredRenderer.h`
- `Source/src/DeferredRenderer.cpp`
- `Source/src/Test5.cpp`
- `Shaders/fragment_DeferredGeometry.glsl`
- `Shaders/fragment_DeferredLighting.glsl`

Added:
- `Shaders/fragment_BRDFLUT.glsl`

## Suggested Visual Checks

- use `OpenGLRasterizer` with environment mapping enabled
- toggle `Specular IBL` on and off to confirm reflections appear and disappear
- increase roughness to confirm reflections blur via mip sampling
- test a metallic-heavy material to confirm stronger colored reflections
