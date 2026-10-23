# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build and Run Commands

### Windows
- **Build Debug**: `.\build-debug-windows.bat`
- **Build Release**: `.\build-release-windows.bat`
- **Run RenderLab**: `.\build\Release\RenderLab.exe`
- **Run FpsGame**: `.\build\Release\FpsGame.exe`

### macOS
- **Setup**: `.\setup-macos.sh`
- **Build Debug**: `.\build-debug-macos.sh`
- **Build Release**: `.\build-release-macos.sh`

### Testing
- **Build Tests**: Included in the main build if `RTRT_BUILD_RENDER_REGRESSION_TESTS` is ON.
- **Run Unit Tests**: `.\build\Release\RenderRegression.exe --unit`
- **Run Specific Test Case**: `.\build\Release\RenderRegression.exe --case <case_name>`
- **CTest**: Run all regression tests with `ctest --test-dir build -C Release --output-on-failure`.
- **Regression Testing**: `RenderRegression` compares current render outputs against ground-truth images.

## Architecture Overview

RTRT is a modular C++17 rendering sandbox and engine exploring multiple rendering pipelines:

### Core Structure
- **RTRTCore (`Engine/`)**: A pure static library containing the core rendering pipelines (Deferred, Path Tracer, Software Rasterizer), scene representation, asset loaders, and math utilities. Decoupled from GUI and game systems.
- **RTRTFramework (`Framework/`)**: Shared application runtime support for interactive projects (KeyInput, MouseInput, NativeFileDialog, RenderStatsUI, dynamic Boids simulation).
- **FpsGame (`Games/FpsGame/`)**: Autonomous FPS game with its own static library (`FpsGameCore`) and standalone executable (`FpsGame.exe`), featuring player physics, projectile simulation, map loading, and an in-game 3D editor.
- **RenderLab (`Apps/RenderLab/`)**: Interactive multi-renderer workbench (`Test5`) and legacy renderer testbed harness (`Test1` through `Test4`).
- **RenderRegression (`Tests/`)**: Headless regression test runner linking exclusively to `RTRTCore` and `nlohmann_json`.

### Rendering Pipeline
The project implements several rendering backends:
- **GPU Path Tracer**: A high-fidelity compute/fragment renderer using GLSL with OIDN denoising.
- **Software Rasterizer**: A CPU-based multithreaded rasterizer with SIMD vectorization (AVX2/NEON).
- **GPU Deferred Renderer**: A modern OpenGL PBR deferred pipeline with G-buffer, shadows, SSAO, SSR, and IBL.

The `RendererFactory` is used to instantiate the desired renderer backend.

### Key Subsystems
- **Scene Management**: Handles meshes, materials, cameras, lights, textures, and BVH acceleration via `Scene` and `Mesh`.
- **Math**: Custom math aliases (`Vec3`, `Mat4x4`, `AABB`, `Ray`) provided in `MathUtil.h` (wrapping GLM).
- **Resource Loading**: Supports loading assets via `Loader` using `tinygltf` and `tinyobjloader`.

### Dependencies
Most dependencies are vendored in the `Dependencies/` directory:
- **Math**: GLM
- **UI**: ImGui, ImGuizmo
- **Windowing/Input**: GLFW
- **OpenGL Loading**: GLEW
- **Asset Loading**: TinyGLTF, TinyObjLoader, TinyDir, NativeFileDialog
- **Acceleration**: RadeonRays, TinyBVH
- **Denoising**: Intel Open Image Denoise (OIDN, fetched automatically via CMake)

## Coding Style

### Naming Conventions
- **Types**: `PascalCase` (e.g., `Camera`, `ShaderProgram`)
- **Functions**: `camelCase` (e.g., `InitializeScene`, `ComputeFrustum`)
- **Data Members**: Leading underscore (e.g., `_Scene`, `_FrameNum`)
- **Function Parameters**: 
  - `iFoo` for input
  - `oFoo` for output
  - `ioFoo` for input/output

### Formatting
- **Spacing**: Use spaces inside parentheses and after control keywords: `if ( condition )`.
- **Braces**: Space before opening braces.
- **Structure**: Implementation files use explicit section banners (e.g., `// ----------------------------------------------------------------------------`).

### Coding Approach
- **Style**: Prefer explicit, imperative code over high abstraction or functional styles.
- **Continuity**: Match the local style of the file being edited. Do not modernize for its own sake.
- **Resources**: OpenGL resources are managed explicitly.
- **Math**: Use project aliases from `MathUtil.h`.
