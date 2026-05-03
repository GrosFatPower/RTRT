# Codex Todo

This file stores assistant-side recommendations and follow-up ideas so they can be reused in later sessions without polluting the project's own `ToDo.txt`.

## Deferred OpenGL Renderer Status

Recently completed foundations in the deferred `OpenGLRasterizer`:

1. Shadow mapping, including `DistantLight` priority with `SphereLight` / `RectLight` fallback
2. SSAO with optional blur and deferred debug-buffer output
3. Specular IBL with env-map mip sampling and BRDF LUT support
4. Deferred visible-light rendering for sphere and rect lights

Do not treat shadow mapping, SSAO, or specular IBL as open roadmap items anymore. They are already in the current codebase.

## Remaining Quality Roadmap

Priority order for likely next improvements:

1. TAA or lightweight temporal accumulation
   Reduce shimmering and stabilize SSAO, shadows, and specular highlights.
2. Manage transparency (windows, glasses, etc.)
   Add a clear deferred/OpenGL track for transparent surfaces such as windows and glass materials.
3. Contact shadows or screen-space shadows
   Add finer local shadowing for small geometry details.
4. Shadow quality improvements
   Improve filtering and fitting, and consider cascades or multi-light support if larger scenes need it.
5. Bloom and post-process polish
   Use as a finishing pass after lighting stability is stronger.
6. Better reflection support beyond specular IBL
   Consider SSR or another reflection path only after the more foundational items above.

## Recommended Near-Term Order

Recommended next pair:

1. TAA or lightweight temporal accumulation
2. Manage transparency (windows, glasses, etc.)

Reasoning:
- Temporal stabilization will improve the perceived quality of several already-shipped features at once.
- Transparency is the most visible remaining material-system gap in the deferred path.

## Features To Avoid As A First Step

Do not start with screen-space reflections.

Reasoning:
- They are less foundational than temporal stability, transparency, shadows, and AO polish.
- They are more brittle and scene-dependent.
- They are less likely to give the best quality-per-effort result at this stage.
