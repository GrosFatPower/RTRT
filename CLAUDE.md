# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build and Run Commands

### Windows
- **Build Debug**: `.\build-debug-windows.bat`
- **Build Release**: `.\build-release-windows.bat`
- **Run Debug**: `.\build\Debug\RenderLab.exe`
- **Run Release**: `.\build\Release\RenderLab.exe`

### macOS
- **Setup**: `.\setup-macos.sh`
- **Build Debug**: `.\build-debug-macos.sh`
- **Build Release**: `.\build-release-macos.sh`

### Testing
- **Build Tests**: Included in the main build if `RTRT_BUILD_RENDER_REGRESSION_TESTS` is ON.
- **Run Unit Tests**: `.\build\Debug\RenderRegression.exe --unit`
- **Run Specific Test Case**: `.\build\Debug\RenderRegression.exe --case <case_name>`
- **CTest**: Integrated into the CMake build system.
- **Regression Testing**: `RenderRegression` compares current render outputs against ground-truth images.

## Architecture Overview

RTRT (Render Lab) is a C++17 project exploring various rendering techniques.

### Core Structure
- **RTRTCore**: A static library containing the shared rendering logic and utilities.
- **RenderLab**: The primary executable providing a UI for interacting with different renderers.
- **RenderRegression**: A tool for comparing render outputs against ground truth images for regression testing.

### Rendering Pipeline
The project implements several rendering backends:
- **GPU Path Tracer**: A high-fidelity renderer using GLSL.
- **Software Rasterizer**: A CPU-based rasterizer implementing a basic Phong reflection model.
- **GPU Deferred Renderer**: A modern GPU pipeline for efficient lighting.
- **GPU Ray Tracer**: An earlier implementation of ray tracing.

The `RendererFactory` is used to instantiate the desired renderer backend.

### Key Subsystems
- **Scene Management**: Handles meshes, materials, cameras, and lights via `Scene` and `Mesh` classes.
- **Math**: Custom math aliases (`Vec3`, `Mat4x4`) provided in `MathUtil.h` (wrapping GLM).
- **FPS Game**: A basic FPS implementation (`FpsGame`) using the rendering backends, including collision detection, HUD, and a simple editor.
- **Resource Loading**: Supports loading assets via `Loader` using `tinygltf` and `tinyobjloader`.

### Dependencies
Most dependencies are vendored in the `Dependencies/` directory:
- **Math**: GLM
- **UI**: ImGui, ImGuizmo
- **Windowing/Input**: GLFW
- **OpenGL Loading**: GLEW
- **Asset Loading**: TinyGLTF, TinyObjLoader, TinyDir, NativeFileDialog
- **Acceleration**: RadeonRays, TinyBVH

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
- **Structure**: Implementation files use explicit section banners (e.g., `// --- CTOR ---`).

### Coding Approach
- **Style**: Prefer explicit, imperative code over high abstraction or functional styles.
- **Continuity**: Match the local style of the file being edited. Do not modernize for its own sake.
- **Resources**: OpenGL resources are managed explicitly.
- **Math**: Use project aliases from `MathUtil.h`.
