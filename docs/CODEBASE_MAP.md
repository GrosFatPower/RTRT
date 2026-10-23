# Codebase Map

This document records the current navigation map of the repository so it can be reloaded quickly in future sessions.

## Top-Level Mental Model

The project is structured into modular layers isolating the core rendering engine from interactive testbeds and gameplay code:
- **`Engine/`** (`RTRTCore`): Standalone core rendering and scene engine (Math, Scene, Loaders, DeferredRenderer, PathTracer, SoftwareRasterizer, JobSystem, ProceduralMesh). Zero dependency on GUI or game code.
- **`Framework/`** (`RTRTFramework`): Shared application runtime support (KeyInput, MouseInput, NativeFileDialog, RenderStatsUI, Boids flocking simulation).
- **`Games/FpsGame/`** (`FpsGameCore` & `FpsGame.exe`): Autonomous first-class FPS game with player physics, map loading, projectile pooling, 3D editor mode, and standalone executable runner (`FpsGameApp`).
- **`Apps/RenderLab/`** (`RenderLab.exe`): Interactive rendering workbench (`Test5`) and renderer testbed launcher (`Test1` through `Test4`).
- **`Tests/`** (`RenderRegression.exe`): Automated headless/offscreen render validation and golden-image comparison suite. Links only to `RTRTCore` and `nlohmann_json`.

## Entry Flow

1. `Apps/RenderLab/src/main.cpp`
   Creates the GLFW/OpenGL window, initializes the test-selection UI, and launches one of the test harnesses (Test1..Test5).
2. `Apps/RenderLab/src/Test5.h` / `Apps/RenderLab/src/Test5.cpp`
   Current architectural center for renderer development. Handles input, UI, scene selection, background selection, live renderer switching, capture requests, and dirty-state notifications.
3. `Games/FpsGame/src/main.cpp` and `Games/FpsGame/src/FpsGameApp.cpp`
   Autonomous FPS game application. Handles first-person input, HUD/debug UI, in-game 3D editor, renderer mode switching, capture requests, and game-scene synchronization.
4. `Engine/src/Renderer.h`
   Declares the renderer lifecycle abstraction used across all applications.
5. `Engine/src/Scene.h` and `Engine/src/Loader.h`
   Provide the shared scene representation and loading path used by all renderers.

## Current Important Subsystems

### Viewer And Orchestration

- `Apps/RenderLab/src/Test5.cpp`
- `Framework/src/KeyInput.h`, `Framework/src/KeyInput.cpp`
- `Framework/src/MouseInput.h`, `Framework/src/MouseInput.cpp`
- `Framework/src/RenderStatsUI.h`, `Framework/src/RenderStatsUI.cpp`

Purpose:
- Own the current scene and renderer.
- Route UI events and camera/light/material editing.
- Switch between renderer implementations without changing the surrounding app model.
- Optionally run the CPU boids simulation and expose it as ordinary dynamic mesh instances.

### FPS Game Project

- `Games/FpsGame/src/FpsGameApp.h`, `Games/FpsGame/src/FpsGameApp.cpp`
- `Games/FpsGame/src/FpsGame.h`, `Games/FpsGame/src/FpsGame.cpp`
- `Games/FpsGame/src/FpsCollision.h`, `Games/FpsGame/src/FpsCollision.cpp`
- `Games/FpsGame/src/FpsGameMap.h`, `Games/FpsGame/src/FpsGameMap.cpp`
- `Games/FpsGame/src/FpsGameEditor.h`, `Games/FpsGame/src/FpsGameEditor.cpp`
- `Games/FpsGame/src/FpsGameHud.h`, `Games/FpsGame/src/FpsGameHud.cpp`
- `Games/FpsGame/src/FpsHeadBob.h`, `Games/FpsGame/src/FpsHeadBob.cpp`
- `Games/FpsGame/src/FpsProjectiles.h`, `Games/FpsGame/src/FpsProjectiles.cpp`
- `Engine/src/ProceduralMesh.h`, `Engine/src/ProceduralMesh.cpp`

Purpose:
- Build a procedural arena scene with reusable scene meshes, materials, and lights.
- Drive first-person movement, jumping, collision, mouse capture, HUD/debug UI, and renderer mode switching.
- Simulate pooled bouncing projectiles with ammo/cooldown state and expose projectile transforms as ordinary mesh instances.
- Bind a simple view weapon and procedural gameplay objects into the shared `Scene` path used by Deferred, Software, and PathTracer photo mode.

### Shared Scene Representation

- `Engine/src/Scene.h`, `Engine/src/Scene.cpp`
- `Engine/src/Camera.h`, `Engine/src/Camera.cpp`
- `Engine/src/Light.h`
- `Engine/src/Material.h`
- `Engine/src/Mesh.h`, `Engine/src/Mesh.cpp`
- `Engine/src/MeshInstance.h`
- `Engine/src/Primitive.h`, `Engine/src/PrimitiveInstance.h`
- `Engine/src/Texture.h`, `Engine/src/Texture.cpp`
- `Engine/src/EnvMap.h`, `Engine/src/EnvMap.cpp`
- `Engine/src/GpuBvh.h`, `Engine/src/GpuBvh.cpp`

Purpose:
- Store camera, lights, textures, materials, meshes, primitives, and environment maps.
- Build compiled geometry, packed texture arrays, and TLAS/BLAS data needed by renderers.

### Scene Loading

- `Engine/src/Loader.h`, `Engine/src/Loader.cpp`
- `Engine/src/Util.h`, `Engine/src/Util.cpp`
- `Engine/src/PathUtils.h`, `Engine/src/PathUtils.cpp`
- `Engine/src/DroppedFileUtils.h`, `Engine/src/DroppedFileUtils.cpp`

Purpose:
- Load custom `.scene` descriptions.
- Load `gltf` and `glb` scenes.
- Translate imported assets into the internal `Scene` representation.
- Parse scene-level render settings and optional environment-map references.
- Locate assets, backgrounds, and shaders using the repo-relative path helpers.

### Renderer Abstraction

- `Engine/src/Renderer.h`, `Engine/src/Renderer.cpp`
- `Engine/src/RendererFactory.h`, `Engine/src/RendererFactory.cpp`
- `Engine/src/RenderSettings.h`

Purpose:
- Define the renderer lifecycle:
  initialize, update, finalize frame, render to texture, render to screen, render to file.
- Carry shared render settings and dirty-state notifications.

## Main Renderers

### GPU Path Tracer

Core files:
- `Engine/src/PathTracer.h`
- `Engine/src/PathTracer.cpp`

Main responsibilities:
- Upload packed scene and BVH data to GPU buffers/textures.
- Run the path tracing pass.
- Accumulate frames across time.
- Denoise with compute shader on Windows or fragment fallback on macOS.
- Present final output to screen.

Key shader files:
- `Shaders/fragment_PathTracer.glsl`
- `Shaders/fragment_Accumulate.glsl`
- `Shaders/compute_Denoiser.glsl`
- `Shaders/fragment_DenoiserPathTracer.glsl`
- `Shaders/fragment_PostProcess.glsl`
- Shared includes: `Shaders/BVH.glsl`, `Shaders/Intersections.glsl`, `Shaders/DisneyBSDF.glsl`, `Shaders/Material.glsl`, `Shaders/Lights.glsl`, `Shaders/Sampling.glsl`

### CPU Software Rasterizer

Core files:
- `Engine/src/SoftwareRasterizer.h`, `Engine/src/SoftwareRasterizer.cpp`
- `Engine/src/RasterData.h`
- `Engine/src/SoftwareVertexShader.h`, `Engine/src/SoftwareVertexShader.cpp`
- `Engine/src/SoftwareFragmentShader.h`, `Engine/src/SoftwareFragmentShader.cpp`
- `Engine/src/SutherlandHodgman.h`, `Engine/src/SutherlandHodgman.cpp`
- `Engine/src/SIMDUtils.h`, `Engine/src/SIMDUtils.hxx`

Main responsibilities:
- CPU-side vertex processing, clipping, rasterization, fragment shading, tiling, and image-buffer assembly.
- Optional SIMD acceleration (AVX2/NEON) and multithreaded execution via `JobSystem`.

### Deferred OpenGL Renderer

Core files:
- `Engine/src/DeferredRenderer.h`
- `Engine/src/DeferredRenderer.cpp`

Main responsibilities:
- Build a G-buffer from scene geometry.
- Run shadow-map, SSAO, SSR, deferred-lighting, transparent-forward, wireframe, and fullscreen composite passes.
- Support deferred debug-buffer views, visible light drawing, and wireframe overlay.
- PBR direct lighting plus specular IBL, screen-space reflections for low-roughness opaque surfaces.

## Historical Tests (Under `Apps/RenderLab/`)

- `Apps/RenderLab/src/Test1.cpp`: Early render-to-texture fullscreen shader playground.
- `Apps/RenderLab/src/Test2.cpp`: Scene-loader inspection harness.
- `Apps/RenderLab/src/Test3.cpp`: Older monolithic GPU ray tracer.
- `Apps/RenderLab/src/Test4.cpp`: Older direct CPU rasterizer harness.

## Fast Navigation Guide

If the task is about:
- App behavior or UI: start with `Apps/RenderLab/src/Test5.cpp`
- FPS gameplay, projectiles, HUD, view weapon: start with `Games/FpsGame/src/FpsGameApp.cpp` and `Games/FpsGame/src/FpsGame.cpp`
- Renderer lifecycle or adding a backend: start with `Engine/src/Renderer.h`
- Scene loading bugs or import behavior: start with `Engine/src/Loader.cpp`
- Shared scene data or packed render data: start with `Engine/src/Scene.cpp`
- GPU path tracing: start with `Engine/src/PathTracer.cpp` and the path tracing shaders
- CPU rasterization: start with `Engine/src/SoftwareRasterizer.cpp`
- Deferred rasterization: start with `Engine/src/DeferredRenderer.cpp`
- Dynamic boids overlay: start with `Framework/src/Boids.cpp`
- Build configuration: start with root `CMakeLists.txt` and respective module `CMakeLists.txt`
