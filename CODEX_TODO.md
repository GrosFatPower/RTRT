# Codex Todo

This file stores assistant-side recommendations and follow-up ideas so they can be reused in later sessions without polluting the project's own `ToDo.txt`.

## OpenGL Rasterizer Quality Roadmap

Priority order for improving the current deferred OpenGL renderer:

1. Shadow mapping
   Add self-shadowing first because it gives the strongest immediate quality gain and fits naturally into the existing deferred lighting pass.
2. SSAO
   Add screen-space ambient occlusion to restore contact depth and small-scale grounding that shadow maps usually miss.
3. Better specular IBL and reflection support
   Improve environment-based lighting so materials feel less flat and more coherent with the path tracer.
4. TAA or lightweight temporal accumulation
   Reduce shimmering and stabilize the image over time.
5. Contact shadows or screen-space shadows
   Add finer local shadowing for small geometry details.
6. Bloom and post-process polish
   Use as a finishing pass after lighting and shading fundamentals are stronger.
7. Manage transparency (windows, glasses, etc.)
   Add a future deferred/OpenGL rasterizer track for transparent surfaces such as windows and glass materials.

## Recommended Implementation Order

Recommended first pair:

1. Shadow mapping
2. SSAO

Reasoning:
- Shadow mapping is the best next step because the deferred renderer already has a natural lighting stage where a shadow term can be integrated.
- SSAO complements shadow mapping well by adding small-scale occlusion and contact depth.

## Shadow Mapping Notes

Suggested rollout:

1. Start with a single shadow map for one main light.
2. Add PCF filtering.
3. Define a manageable policy for shadow-casting lights.
   Example: one primary light first, then broaden support later.
4. Use this as the basis for later improvements such as softer filtering or cascades.

## Features To Avoid As A First Step

Do not start with screen-space reflections.

Reasoning:
- They are less foundational than shadows and AO.
- They are more brittle and scene-dependent.
- They are less likely to give the best quality-per-effort result at this stage.
