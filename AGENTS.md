# AGENTS.md

## Project Context

This repository is a C++ rendering sandbox/workbench centered on shared scene loading and multiple renderer backends. The current architectural center is `Test5` plus the `Renderer` subclasses (`PathTracer`, `SoftwareRasterizer`, `DeferredRenderer`).

## Quick Codebase Map

Use this as the fast navigation guide.

- App entry: `Source/src/main.cpp`
- Current viewer/controller: `Source/src/Test5.h` and `Source/src/Test5.cpp`
- Shared renderer abstraction: `Source/src/Renderer.h` and `Source/src/Renderer.cpp`
- Scene model and compiled render data: `Source/src/Scene.h` and `Source/src/Scene.cpp`
- Scene loading: `Source/src/Loader.h` and `Source/src/Loader.cpp`
- GPU path tracing: `Source/src/PathTracer.h`, `Source/src/PathTracer.cpp`, and the path tracing shaders in `Shaders/`
- CPU software rasterization: `Source/src/SoftwareRasterizer.h` and `Source/src/SoftwareRasterizer.cpp`
- Deferred OpenGL renderer: `Source/src/DeferredRenderer.h` and `Source/src/DeferredRenderer.cpp`
- Core math and shared data types: `Source/src/MathUtil.h`, `Camera.h`, `Light.h`, `Material.h`, `Mesh.h`, `Texture.h`, `GpuBvh.h`, `RenderSettings.h`
- Assets and scene definitions: `Assets/`
- GLSL programs and shared shader includes: `Shaders/`
- Third-party libraries: `Dependencies/`

## House Style

Follow the existing project style unless the user explicitly asks for a different direction.

- Prefer classic `.h` / `.cpp` separation with include guards such as `_Camera_` rather than `#pragma once`.
- Use `PascalCase` for types, `camelCase` for functions, and leading-underscore member names such as `_Scene`, `_FrameNum`, `_RenderResolution`.
- Keep the existing parameter naming convention:
  `iFoo` for input, `oFoo` for output, `ioFoo` for in/out.
- Match the repo's spacing and brace style:
  spaces inside parentheses, space before braces, compact one-line getters when trivial.
- Preserve the section-banner convention in implementation files when editing substantial code blocks:
  `// ----------------------------------------------------------------------------`
- Prefer direct, readable, imperative control flow over clever abstractions.
- Keep comments sparse and purposeful. Use them mainly for section labels, intent, or non-obvious rendering steps.
- Favor explicit loops and indexing over dense STL-heavy rewrites unless there is a clear benefit.
- Respect the mixed ownership style already present in the repo:
  older areas may use raw pointers and explicit cleanup, newer areas may use `std::unique_ptr` / `std::shared_ptr`.
- For engine-style data containers such as render settings, materials, and GL resource wrappers, prefer the repo's existing plain-struct/public-field style.
- Preserve historical continuity. Do not refactor older code into a radically different style unless the user asks for it.

## Working Preferences

- Optimize for debuggability and explicitness.
- Keep rendering stages easy to follow: load, compile, bind, update, render, present.
- Keep domain logic close to usage unless abstraction clearly improves the code.
- When extending architecture, prefer the pattern already used in the project rather than introducing a new framework or style.
