# Codebase Map

This document records the current navigation map of the repository so it can be reloaded quickly in future sessions.

## Top-Level Mental Model

The project is a C++ rendering sandbox that evolved from isolated rendering experiments into a shared viewer and gameplay workbench able to drive multiple renderer backends over the same scene representation.

Main pattern:
- `main.cpp` opens the app and selects a test.
- `Test5` is the current interactive viewer/controller.
- `Test6` is the current FPS-game sandbox built on the same scene and renderer abstraction.
- `Scene` stores loaded assets plus compiled render data.
- `Renderer` defines the common rendering lifecycle.
- `PathTracer`, `SoftwareRasterizer`, and `DeferredRenderer` are the main renderer implementations.
- Root `CMakeLists.txt` defines the current executable target as `RenderLab`.

## Entry Flow

1. `Source/src/main.cpp`
   Creates the GLFW/OpenGL window, initializes the test-selection UI, and launches one of the test harnesses.
2. `Source/src/Test5.h` / `Source/src/Test5.cpp`
   Current architectural center. Handles input, UI, scene selection, background selection, live renderer switching, capture requests, and dirty-state notifications.
3. `Source/src/Test6.h` / `Source/src/Test6.cpp`
   FPS-game sandbox. Handles first-person input, HUD/debug UI, renderer mode switching, capture requests, and game-scene synchronization.
4. `Source/src/Renderer.h`
   Declares the renderer interface used by `Test5`.
5. `Source/src/Scene.h` and `Source/src/Loader.h`
   Provide the shared scene representation and loading path used by all renderers.

## Current Important Subsystems

### Viewer And Orchestration

- `Source/src/Test5.cpp`
- `Source/src/Test6.cpp`
- `Source/src/KeyInput.h`, `Source/src/KeyInput.cpp`
- `Source/src/MouseInput.h`, `Source/src/MouseInput.cpp`

Purpose:
- Own the current scene and renderer.
- Route UI events and camera/light/material editing.
- Switch between renderer implementations without changing the surrounding app model.
- Optionally run the CPU boids simulation and expose it as ordinary dynamic mesh instances.
- Run the Test6 FPS sandbox using the same renderer interface.

### FPS Game Sandbox

- `Source/src/Test6.h`
- `Source/src/Test6.cpp`
- `Source/src/FpsGame.h`
- `Source/src/FpsGame.cpp`
- `Source/src/ProceduralMesh.h`
- `Source/src/ProceduralMesh.cpp`

Purpose:
- Build a procedural arena scene with reusable scene meshes, materials, and lights.
- Drive first-person movement, jumping, collision, mouse capture, HUD/debug UI, and renderer mode switching.
- Simulate pooled bouncing projectiles with ammo/cooldown state and expose projectile transforms as ordinary mesh instances.
- Bind a simple view weapon and procedural gameplay objects into the shared `Scene` path used by Deferred, Software, and PathTracer photo mode.

### Shared Scene Representation

- `Source/src/Scene.h`
- `Source/src/Scene.cpp`
- `Source/src/Camera.h`
- `Source/src/Light.h`
- `Source/src/Material.h`
- `Source/src/Mesh.h`
- `Source/src/MeshInstance.h`
- `Source/src/Primitive.h`
- `Source/src/PrimitiveInstance.h`
- `Source/src/Texture.h`
- `Source/src/EnvMap.h`
- `Source/src/GpuBvh.h`

Purpose:
- Store camera, lights, textures, materials, meshes, primitives, and environment maps.
- Build compiled geometry, packed texture arrays, and TLAS/BLAS data needed by renderers.

### Scene Loading

- `Source/src/Loader.h`
- `Source/src/Loader.cpp`
- `Source/src/Util.h`
- `Source/src/Util.cpp`
- `Source/src/PathUtils.h`

Purpose:
- Load custom `.scene` descriptions.
- Load `gltf` and `glb` scenes.
- Translate imported assets into the internal `Scene` representation.
- Parse scene-level render settings and optional environment-map references.
- Locate assets, backgrounds, and shaders using the repo-relative path helpers.

### Renderer Abstraction

- `Source/src/Renderer.h`
- `Source/src/Renderer.cpp`
- `Source/src/RenderSettings.h`

Purpose:
- Define the renderer lifecycle:
  initialize, update, finalize frame, render to texture, render to screen, render to file.
- Carry shared render settings and dirty-state notifications.

## Main Renderers

### GPU Path Tracer

Core files:
- `Source/src/PathTracer.h`
- `Source/src/PathTracer.cpp`

Main responsibilities:
- Upload packed scene and BVH data to GPU buffers/textures.
- Run the path tracing pass.
- Accumulate frames across time.
- Optionally denoise. Windows uses the compute shader path; macOS and other non-Windows OpenGL 4.1 targets use a fullscreen fragment fallback.
- Track GPU pass timings. Windows uses timestamp query pairs; macOS uses `GL_TIME_ELAPSED`.
- Present the final output to screen.

Key shader files:
- `Shaders/fragment_PathTracer.glsl`
- `Shaders/fragment_Accumulate.glsl`
- `Shaders/compute_Denoiser.glsl`
- `Shaders/fragment_DenoiserPathTracer.glsl`
- `Shaders/fragment_PostProcess.glsl`
- Shared includes such as `Shaders/BVH.glsl`, `Shaders/Intersections.glsl`, `Shaders/DisneyBSDF.glsl`, `Shaders/Material.glsl`, `Shaders/Lights.glsl`, `Shaders/Sampling.glsl`

### CPU Software Rasterizer

Core files:
- `Source/src/SoftwareRasterizer.h`
- `Source/src/SoftwareRasterizer.cpp`
- `Source/src/RasterData.h`
- `Source/src/SIMDUtils.h`
- `Source/src/SIMDUtils.hxx`

Main responsibilities:
- CPU-side vertex processing, clipping, rasterization, fragment shading, tiling, and image-buffer assembly.
- Optional SIMD acceleration and multithreaded execution.
- Output upload and presentation.

### Deferred OpenGL Renderer

Core files:
- `Source/src/DeferredRenderer.h`
- `Source/src/DeferredRenderer.cpp`

Main responsibilities:
- Build a G-buffer from scene geometry.
- Run shadow-map, SSAO, SSR, deferred-lighting, transparent-forward, wireframe, and fullscreen composite passes.
- Support deferred debug-buffer views, visible light drawing, and wireframe overlay.
- Manage GPU-side texture filtering, env-map mip usage, BRDF LUT generation, and anisotropy.
- Use PBR direct lighting plus specular IBL, with screen-space reflections for low-roughness opaque surfaces.
- Support multiple selected shadow-casting lights. Distant lights use a 2D depth texture array, while sphere and rect lights use cubemap-array layers.
- Split opaque and transparent mesh-instance rendering, including per-triangle sorting for transparent meshes.

Key shader files:
- `Shaders/vertex_DeferredGeometry.glsl`
- `Shaders/fragment_DeferredGeometry.glsl`
- `Shaders/fragment_DeferredLighting.glsl`
- `Shaders/DeferredPBRLighting.glsl`
- `Shaders/PBR.glsl`
- `Shaders/Shadows.glsl`
- `Shaders/fragment_SSAO.glsl`
- `Shaders/fragment_SSAOBlur.glsl`
- `Shaders/fragment_SSR.glsl`
- `Shaders/fragment_BRDFLUT.glsl`
- `Shaders/fragment_DeferredTransparent.glsl`
- `Shaders/vertex_ShadowCubeDepth.glsl`
- `Shaders/fragment_ShadowCubeDepth.glsl`
- `Shaders/vertex_ShadowDirectionalDepth.glsl`
- `Shaders/fragment_ShadowDirectionalDepth.glsl`
- `Shaders/fragment_Output.glsl`

## Historical Tests

These remain useful for understanding how the project evolved, but they are no longer the main architectural center.

- `Source/src/Test1.cpp`
  Early render-to-texture fullscreen shader playground.
- `Source/src/Test2.cpp`
  Scene-loader inspection harness.
- `Source/src/Test3.cpp`
  Older monolithic GPU ray tracer.
- `Source/src/Test4.cpp`
  Older direct CPU rasterizer harness.

## Active Test Harnesses

- `Source/src/Test5.cpp`
  Main scene viewer/editor for loaded assets, renderer comparison, material/light/mesh editing, boids, and capture workflows.
- `Source/src/Test6.cpp`
  FPS-game sandbox for real-time renderer/gameplay integration, procedural arena content, projectiles, HUD/debug UI, and PathTracer photo mode.

## Shared Utility Layer

Important utility files include:
- `Source/src/MathUtil.h`
- `Source/src/GLUtil.h`
- `Source/src/Shader.h`
- `Source/src/Shader.cpp`
- `Source/src/ShaderProgram.h`
- `Source/src/ShaderProgram.cpp`
- `Source/src/QuadMesh.h`
- `Source/src/QuadMesh.cpp`
- `Source/src/Boids.h`
- `Source/src/Boids.cpp`
- `Source/src/FpsGame.h`
- `Source/src/FpsGame.cpp`
- `Source/src/ProceduralMesh.h`
- `Source/src/ProceduralMesh.cpp`
- `Source/src/JobSystem.h`
- `Source/src/JobSystem.cpp`

These provide the math aliases, GL helper wrappers, shader compilation/linking, common fullscreen geometry, optional CPU-side dynamic boid scene binding, procedural gameplay mesh creation, and lightweight job support used throughout the project.

## Asset And Data Areas

- `Assets/`
  Scene descriptions, meshes, textures, HDR environment maps, and bundled source assets.
- `Shaders/`
  GLSL passes and shared include files.
- `Dependencies/`
  Vendored third-party libraries such as GLFW, GLEW, GLM, ImGui, tinygltf, tinyobjloader, stb, and BVH code.
- `Captures/`
  Reference outputs and comparison renders.
- `Resources/`
  Supporting documents, notes, and external references.

## Fast Navigation Guide

If the task is about:
- App behavior or UI: start with `Source/src/Test5.cpp`
- FPS gameplay, projectiles, HUD, view weapon, or game-scene binding: start with `Source/src/Test6.cpp` and `Source/src/FpsGame.cpp`
- Renderer lifecycle or adding a backend: start with `Source/src/Renderer.h`
- Scene loading bugs or import behavior: start with `Source/src/Loader.cpp`
- Shared scene data or packed render data: start with `Source/src/Scene.cpp`
- GPU path tracing: start with `Source/src/PathTracer.cpp` and the path tracing shaders
- Path tracer denoising/timing: start with `Source/src/PathTracer.cpp`, `Shaders/compute_Denoiser.glsl`, and `Shaders/fragment_DenoiserPathTracer.glsl`
- CPU rasterization: start with `Source/src/SoftwareRasterizer.cpp`
- Deferred rasterization: start with `Source/src/DeferredRenderer.cpp`
- Deferred SSR: start with `Source/src/DeferredRenderer.cpp` and `Shaders/fragment_SSR.glsl`
- Deferred shadows: start with `Source/src/DeferredRenderer.cpp`, `Shaders/Shadows.glsl`, and the shadow depth shaders
- Deferred transparency: start with `Source/src/DeferredRenderer.cpp` and `Shaders/fragment_DeferredTransparent.glsl`
- Dynamic boids overlay: start with `Source/src/Boids.cpp` and `Source/src/Test5.cpp`
- Build configuration: start with root `CMakeLists.txt`, which is the source of truth for the `RenderLab` target

## Current Architectural Center

When there is any doubt about where to begin, prioritize this path:
- `Source/src/main.cpp`
- `Source/src/Test5.cpp`
- `Source/src/Test6.cpp` when the task is gameplay-facing
- `Source/src/Renderer.h`
- `Source/src/Scene.h`
- `Source/src/Loader.cpp`
- selected renderer implementation

That route reflects the current shape of the codebase more accurately than the earlier test harnesses. For renderer-only work, start from `Test5`; for playable real-time integration, start from `Test6` and `FpsGame`.
