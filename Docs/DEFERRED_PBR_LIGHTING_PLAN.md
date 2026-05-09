# Deferred PBR Lighting Plan

## Summary
Implement deferred PBR direct lighting with a modular Cook-Torrance GGX BRDF. The implementation reuses the existing shared material fields: albedo, metallic, roughness, reflectance, emission, opacity/mask handling, and texture-resolved values from the geometry pass.

The first drop uses pragmatic sphere and rect light approximations instead of LTC area lights. This keeps the code compact, debuggable, and fast while replacing the current Blinn-style deferred direct lighting.

## Chosen BRDF
- GGX/Trowbridge-Reitz normal distribution.
- Smith/Schlick-GGX visibility.
- Schlick Fresnel.
- Energy-conserving Lambert diffuse: `(1 - F) * (1 - metallic) * albedo / PI`.
- Cook-Torrance specular: `D * G * F / max(4 * NdotV * NdotL, EPSILON)`.
- Shared F0 convention: `mix(vec3(0.16 * reflectance * reflectance), albedo, metallic)`.

## Implementation Notes
- Add a reusable `Shaders/PBR.glsl` include for BRDF evaluation.
- Add a small deferred light helper include for distant, sphere, and rect light sampling.
- Add an emission G-buffer target so opaque material emission participates in deferred lighting.
- Replace the deferred direct lighting loop with PBR diffuse/specular accumulation.
- Keep shadows applied to direct radiance.
- Keep SSAO on diffuse/ambient-style terms, not as a heavy raw specular multiplier.
- Keep specular IBL and SSR as indirect/screen-space specular contributions.
- Update transparent forward lighting to use the same PBR helper.
- Add direct diffuse and direct specular debug views.
- Add UI controls for PBR direct lighting and direct light intensity.

## Limitations
- Sphere and rect lights are approximated with representative direction and simple distance/area attenuation.
- Rect lights do not use LTC lookup tables yet.
- Transparent transmission/refraction remains separate from this direct-lighting pass.
- Existing scene light intensities may need retuning because the old deferred path normalized light emission and clamped direct lighting.

## Validation Checklist
- Build with `cmake --build build`.
- Verify Test5 deferred scenes with rough/smooth dielectric and metallic materials.
- Verify shadows, SSAO, specular IBL, SSR, transparent forward pass, resize, and render-to-file.
- Verify Test6 still reads well with projectiles, glossy floor, SSR, and renderer switching.
- Compare qualitatively with SoftwareRasterizer and PathTracer for the same material settings.
