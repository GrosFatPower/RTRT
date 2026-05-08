# Deferred Screen Space Reflections Recap

## Summary
First-drop screen space reflections are implemented in the deferred OpenGL renderer. The feature is intentionally basic but usable for Test6 experimentation: low-roughness opaque surfaces can reflect visible screen-space geometry, with Test5 kept unchanged by default.

SSR is a standalone fullscreen pass that writes an HDR reflection buffer. Deferred lighting samples that buffer and blends valid reflections into the specular term. A previous-frame lighting source texture is used to avoid feedback hazards when sampling reflected scene color.

## What Was Implemented
- Added deferred SSR settings in `RenderSettings`: enable flag, max steps, step size, max distance, thickness, intensity, max roughness, and edge fade.
- Added `Docs/SSR_DEFERRED_PLAN.md` as the implementation trace.
- Added deferred renderer resources:
  - `_SSRTEX` / `_SSRFBO` for the SSR result.
  - `_SSRSourceTEX` / `_SSRSourceFBO` for previous-frame reflected color source.
- Added renderer methods:
  - `InitializeSSR()`
  - `BindSSRPassTextures()`
  - `RenderSSR()`
  - `UpdateSSRSource()`
- Integrated the pass in the deferred frame order:
  - shadow maps
  - opaque G-buffer
  - SSAO
  - SSR
  - deferred lighting
  - transparent forward pass
  - wire overlay
  - composite
- Added `Shaders/fragment_SSR.glsl`.
- Added SSR blending and the `SSR` debug buffer view in `fragment_DeferredLighting.glsl`.
- Added SSR UI controls and debug-view entries to Test5 and Test6.
- Enabled SSR by default in Test6 only.
- Tuned some Test6 materials so the floor, accent blocks, and projectiles are glossy enough to exercise SSR.

## Current Behavior
- SSR uses the opaque G-buffer position, normal, material, and depth.
- Reflections are only generated for pixels under `_SSRMaxRoughness`.
- Rays use signed view-space depth crossing to reduce same-surface self-hits.
- A small stable per-pixel jitter offsets the ray start to reduce coherent marching stripes.
- SSR history is updated only from normal color frames, so debug buffer views do not pollute the next reflection source.
- The SSR debug view shows black background and makes hit confidence visible even when the reflected source is dark.

## Known Limitations
- Screen-space only: hidden, occluded, off-screen, and transparent objects cannot appear in reflections.
- The reflected color comes from previous-frame lighting, so fast camera/object motion can show lag or stale color.
- The current ray marcher uses fixed world-space steps, which can still create angle-dependent banding or jitter.
- No bilateral blur, denoising, temporal accumulation, history rejection, hierarchical Z, or half-resolution optimization yet.
- Transparent objects neither contribute to SSR nor receive SSR in v1.
- SSR quality is scene-scale sensitive; `SSR step size`, `SSR thickness`, and `SSR max steps` need tuning per scene.

## Todo List
- Add an optional bilateral blur pass for the SSR buffer, guided by depth and normal, to reduce jitter without bleeding across edges.
- Consider screen-space ray stepping instead of fixed world-space stepping for more uniform sample distribution.
- Add a confidence/mask-only SSR debug mode separate from raw reflected color.
- Add temporal accumulation with history rejection based on depth, normal, and motion/camera changes.
- Investigate hierarchical Z tracing for better performance and fewer missed intersections.
- Add half-resolution SSR as a performance option once blur/upscale exists.
- Improve material integration with a clearer reflection strength policy, especially for metallic/reflectance edge cases.
- Add scene/UI presets for conservative, balanced, and high-quality SSR tuning.

## Test Checklist
- Build with `cmake --build build`.
- Confirm Test5 deferred output remains unchanged with SSR disabled.
- In Test5, enable SSR and verify reflective low-roughness materials, roughness fade, resize, debug view, and render-to-file.
- In Test6, verify SSR starts enabled and floor/accent/projectile reflections are visible.
- Toggle SSR and confirm disabling restores the previous deferred output.
- Check SSR debug view for black background, visible hit regions, and no debug-history pollution.
- Confirm SSAO, shadows, specular IBL, transparency, wireframe overlay, renderer switching, and existing debug views still work.
- Watch for GL feedback warnings or shader compilation errors after shader edits.
