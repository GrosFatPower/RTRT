# Codex Todo

This file stores assistant-side recommendations and follow-up ideas so they can be reused in later sessions without polluting the project's own `ToDo.txt`.

## Deferred OpenGL Renderer Status

Recently completed foundations in the deferred `OpenGLRasterizer`:

1. Shadow mapping, including multiple selected shadow-casting lights, `DistantLight` support, and `SphereLight` / `RectLight` cubemap-array support
2. SSAO with optional blur and deferred debug-buffer output
3. Specular IBL with env-map mip sampling and BRDF LUT support
4. First-drop SSR for low-roughness opaque surfaces, with a previous-frame source texture and SSR debug-buffer output
5. PBR direct lighting shared by deferred and transparent lighting paths
6. Deferred visible-light rendering for sphere and rect lights
7. Transparency for mesh instances, including `AlphaMode::Blend`, transmissive material routing, and per-triangle sorting for transparent meshes

Do not treat shadow mapping, SSAO, specular IBL, SSR, PBR direct lighting, or deferred mesh-instance transparency as open roadmap items anymore. They are already in the current codebase.

## Test6 Status

`Test6` is now an active FPS-game sandbox, not just a plan. It includes:

1. Procedural arena scene binding through the shared `Scene` path
2. First-person movement, jumping, collision, mouse capture, HUD, and debug UI
3. Deferred, Software, and PathTracer photo renderer modes
4. Pooled bouncing projectiles with ammo/cooldown state
5. Procedural sphere projectile meshes and a bound view weapon

## Path Tracer Status

Recently completed path tracer maintenance:

1. Denoising is active on macOS through a GLSL 410 fullscreen fragment fallback while Windows keeps the compute shader path.
2. Path trace, accumulate, denoise, and render-to-screen timers use platform-appropriate GPU queries: timestamp pairs on Windows and `GL_TIME_ELAPSED` on macOS.
3. Boids plus denoising no longer freezes because denoise timing is read only when the denoise pass actually ran.

## Remaining Quality Roadmap

Priority order for likely next improvements:

1. TAA or lightweight temporal accumulation
   Reduce shimmering and stabilize SSAO, shadows, SSR, and specular highlights.
2. Contact shadows or screen-space shadows
   Add finer local shadowing for small geometry details.
3. Shadow quality improvements
   Improve filtering and fitting, and consider cascades or tighter caster selection if larger scenes need it.
4. Bloom and post-process polish
   Use as a finishing pass after lighting stability is stronger.
5. SSR polish
   Add bilateral blur, better confidence/mask visualization, temporal rejection, or hierarchical-Z tracing if reflections become a priority.
6. Transparency quality improvements
   Consider OIT, depth peeling, or screen-space refraction only if transparent scene requirements outgrow the current sorted forward pass.

## Recommended Near-Term Order

Recommended next pair:

1. TAA or lightweight temporal accumulation
2. Contact shadows or screen-space shadows

Reasoning:
- Temporal stabilization will improve the perceived quality of several already-shipped features at once.
- Contact shadows would add local detail without starting with the more brittle reflection work.

## Reflection Work Scope

SSR already exists as a first-drop feature. Treat future reflection work as polish or quality work, not initial implementation.

Recommended SSR follow-ups:

1. Add a bilateral blur pass guided by depth and normals.
2. Add confidence or mask-only debug output separate from reflected color.
3. Add temporal accumulation with history rejection after the broader TAA/history policy is clearer.
4. Consider hierarchical-Z tracing only after simpler stability improvements are exhausted.
