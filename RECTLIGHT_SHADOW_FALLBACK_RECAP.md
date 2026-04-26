# RectLight Shadow Fallback Recap

Date: 2026-03-08
Feature: `RectLight` fallback for deferred shadow mapping

## Current Status Note

This recap still reflects the rect-light fallback work, but current code now gives `DistantLight` priority ahead of both `SphereLight` and `RectLight`.

The current CMake executable target name is `RenderLab`.

## Goal

Extend the deferred `OpenGLRasterizer` shadow-caster selection so scenes that contain no `SphereLight` can still use v1 shadow mapping from a `RectLight`.

## What Changed

Updated `Source/src/DeferredRenderer.cpp`:
- kept `SphereLight` as the first-priority shadow caster
- added a fallback search for the first `RectLight`
- if no sphere light exists, the renderer now uses the rect-light center at `Light._Pos` as the cubemap shadow origin
- left the cubemap shadow pass unchanged otherwise

Updated `Shaders/fragment_DeferredLighting.glsl`:
- allowed the selected shadow-casting light to be either `SPHERE_LIGHT` or `QUAD_LIGHT`
- reused the same cubemap visibility test for the fallback rect-light path

## Behavior After The Change

- If a scene contains at least one `SphereLight`, the first sphere light still casts shadows.
- If there is no sphere light but there is a `RectLight`, the first rect light now casts shadows.
- The rect-light shadow origin is the quad center stored in `Light._Pos`.
- If neither light type exists, the renderer still falls back to unshadowed deferred lighting.

## Verification

Build verification at the time used the then-current target naming.

Current equivalent local build command:
- `cmake --build build --target RenderLab`

Result:
- build succeeded

Note:
- the first non-escalated build attempt failed because the sandbox could not access `C:\Users\GRIFFON\AppData\Local\Microsoft SDKs`
- rerunning the build with escalation succeeded

## Files Changed

Modified:
- `Source/src/DeferredRenderer.cpp`
- `Shaders/fragment_DeferredLighting.glsl`

## Suggested Visual Checks

- test a scene that contains only `RectLight` emitters
- verify that the shadow origin feels centered on the quad
- inspect whether the point-source approximation is acceptable for large area lights
