# OpenGL Rasterizer Light Visibility Recap

Date: 2026-03-08
Feature: make `Show lights` effective in the deferred `OpenGLRasterizer`

## Goal

Make the existing `Show lights` toggle work in the deferred OpenGL renderer, similarly to the path tracer.

## What Changed

Updated `Shaders/fragment_DeferredLighting.glsl`:
- included `Structures.glsl` and `Intersections.glsl`
- added `GetCameraRayDir()` to reconstruct the camera ray for the current pixel
- added `TraceVisibleLight()` to ray-test the current pixel against visible light shapes
- added support for drawing:
  - `SphereLight` as an actual sphere intersection
  - `RectLight` as an actual quad intersection
- kept `DistantLight` unchanged, with no explicit visible shape drawn

## Behavior After The Change

When `u_ShowLights != 0` in the deferred renderer:
- if the current pixel does not hit scene geometry, the shader now checks whether that camera ray hits a sphere light or rect light and displays the light emission color
- if the current pixel does hit scene geometry, the shader checks whether a visible light shape lies in front of that geometry along the camera ray and, if so, displays the light emission color instead of the shaded surface
- if no visible light shape is hit, the shader falls back to the normal deferred shading path

This makes the existing `Show lights` UI toggle effective for the OpenGL rasterizer without adding a separate renderer path or extra draw pass.

## Files Changed

Modified:
- `Shaders/fragment_DeferredLighting.glsl`

## Notes

- This implementation is shader-only and reuses the light data already passed to the deferred lighting shader.
- The displayed light color uses the current light emission value.
- The shadow-mapping logic remains unchanged.
- This version does not attempt to draw a visible representation for `DistantLight`.

## Verification Still Needed

This change was not compile-validated through a C++ build because it is GLSL-only.

Recommended runtime checks:
- enable `Show lights` in `OpenGLRasterizer`
- verify that sphere lights appear as spheres
- verify that rect lights appear as quads
- verify that lights are only shown when visible to the camera ray and are not drawn through occluding geometry
- verify that turning the toggle off restores the standard deferred output
