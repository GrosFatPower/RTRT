# Codex Todo

This file stores assistant-side recommendations and follow-up ideas so they can be reused in later sessions without polluting the project's own `ToDo.txt`.

## Deferred OpenGL Renderer Status

Recently completed foundations in the deferred `OpenGLRasterizer`:

1. Shadow mapping, including `DistantLight` priority with `SphereLight` / `RectLight` fallback
2. SSAO with optional blur and deferred debug-buffer output
3. Specular IBL with env-map mip sampling and BRDF LUT support
4. Deferred visible-light rendering for sphere and rect lights
5. Transparency for mesh instances, including `AlphaMode::Blend`, transmissive material routing, and per-triangle sorting for transparent meshes

Do not treat shadow mapping, SSAO, specular IBL, or deferred mesh-instance transparency as open roadmap items anymore. They are already in the current codebase.

## Path Tracer Status

Recently completed path tracer maintenance:

1. Denoising is active on macOS through a GLSL 410 fullscreen fragment fallback while Windows keeps the compute shader path.
2. Path trace, accumulate, denoise, and render-to-screen timers use platform-appropriate GPU queries: timestamp pairs on Windows and `GL_TIME_ELAPSED` on macOS.
3. Boids plus denoising no longer freezes because denoise timing is read only when the denoise pass actually ran.

## Remaining Quality Roadmap

Priority order for likely next improvements:

1. TAA or lightweight temporal accumulation
   Reduce shimmering and stabilize SSAO, shadows, and specular highlights.
2. Contact shadows or screen-space shadows
   Add finer local shadowing for small geometry details.
3. Shadow quality improvements
   Improve filtering and fitting, and consider cascades or multi-light support if larger scenes need it.
4. Bloom and post-process polish
   Use as a finishing pass after lighting stability is stronger.
5. Better reflection support beyond specular IBL
   Consider SSR or another reflection path only after the more foundational items above.
6. Transparency quality improvements
   Consider OIT, depth peeling, or screen-space refraction only if transparent scene requirements outgrow the current sorted forward pass.

## Recommended Near-Term Order

Recommended next pair:

1. TAA or lightweight temporal accumulation
2. Contact shadows or screen-space shadows

Reasoning:
- Temporal stabilization will improve the perceived quality of several already-shipped features at once.
- Contact shadows would add local detail without starting with the more brittle reflection work.

## Features To Avoid As A First Step

Do not start with screen-space reflections.

Reasoning:
- They are less foundational than temporal stability, shadow/AO polish, and the current sorted transparency pass.
- They are more brittle and scene-dependent.
- They are less likely to give the best quality-per-effort result at this stage.
