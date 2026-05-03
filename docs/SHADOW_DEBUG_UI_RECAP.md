# Shadow Debug UI Recap

Date: 2026-03-08
Feature: deferred shadow debug view moved into the `Buffer` combo

## Current Status Note

This recap still matches the current shadow debug UI behavior.

The deferred `Buffer` combo has since expanded further and now also includes `SSAO`, `Specular IBL`, and `Material Params`.

The current CMake executable target name is `RenderLab`.

## Goal

Replace the standalone `Show shadow factor` checkbox in the `OpenGLRasterizer` UI with a proper `Shadows` entry in the existing `Buffer` dropdown, alongside `Color`, `Depth`, and `Normals`.

## What Changed

Updated `Source/src/Test5.cpp`:
- removed the separate `Show shadow factor` checkbox from the `OpenGLRasterizer` settings block
- extended the deferred `Buffer` combo to show `Color`, `Depth`, `Normals`, and `Shadows`
- kept the software-rasterizer buffer list unchanged
- synchronized `_Settings._ShowShadowMap` from the selected deferred buffer entry so the existing debug-state path still works
- mapped the `Shadows` entry directly to `DeferredDebugModes::Shadows`

## Behavior After The Change

- In `OpenGLRasterizer`, the `Buffer` combo now contains a `Shadows` view.
- Selecting `Shadows` enables the shadow-factor debug output.
- Switching back to `Color`, `Depth`, or `Normals` disables that debug view automatically.
- In `SoftwareRasterizer`, the `Buffer` combo still exposes only `Color`, `Depth`, and `Normals`.

## Verification

Build verification at the time used the then-current target naming.

Current equivalent local build command:
- `cmake --build build --target RenderLab`

Result:
- build succeeded

Observed warning:
- linker warning `LNK4098` about `LIBCMT` conflict was present during the build output
- this warning was not introduced or addressed as part of this UI change

## Files Changed

Modified:
- `Source/src/Test5.cpp`
