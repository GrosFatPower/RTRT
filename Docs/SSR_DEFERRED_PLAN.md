# Deferred Screen Space Reflections Plan

## Summary
Add a first screen space reflections pass to the deferred OpenGL renderer. The pass reuses the existing G-buffer, fullscreen quad, GLUtil descriptor/FBO helpers, render settings, and debug-view UI patterns already used by SSAO and specular IBL.

SSR is implemented as a standalone fullscreen pass that writes an HDR reflection texture. Deferred lighting samples that texture and blends valid reflections into the specular contribution. The feature is disabled by default globally and enabled by default only in Test6.

## Intended API And Settings
- Add deferred-only `RenderSettings` fields for enabling SSR and tuning ray-march cost/quality: max steps, step size, max distance, thickness, intensity, max roughness, and edge fade.
- Add a deferred `SSR` debug mode and expose it in Test5/Test6 deferred debug combos.
- Add deferred renderer resources for an SSR output texture/FBO plus a previous-frame lighting source texture/FBO to avoid GL feedback hazards.
- Add `Shaders/fragment_SSR.glsl` for the ray-march pass.

## Implementation Notes
- Pipeline order: shadow maps, opaque G-buffer, SSAO, SSR, deferred lighting, transparent pass, overlays, composite.
- SSR samples G-buffer position, normal, material, depth, camera/projection uniforms, previous-frame lighting, and environment data.
- The shader early-outs for background pixels, rough surfaces, invalid rays, off-screen rays, and misses.
- Deferred lighting uses SSR only for opaque pixels and fades it by roughness, screen edge, trace confidence, and material reflectance.
- Specular IBL remains the fallback for misses and high roughness.

## V1 Limitations
- Reflections are screen-space only and cannot show hidden or off-screen geometry.
- Transparent objects do not contribute to or receive SSR.
- No temporal filtering, denoising, hierarchical Z, or half-resolution optimization in the first drop.
- The reflected color uses the previous-frame lighting source, so fast camera/object motion may show expected SSR lag/artifacts.

## Test Checklist
- Build with `cmake --build build`.
- Confirm Test5 deferred output is unchanged with SSR disabled.
- Enable SSR in Test5 and verify reflective low-roughness surfaces, roughness fade, resize, debug view, and render-to-file.
- Launch Test6 and verify SSR is enabled by default and can be toggled/tuned.
- Confirm SSAO, shadows, specular IBL, transparency, wireframe overlay, renderer switching, and existing debug views still work.
- Verify no feedback warnings from sampling the active lighting target.
