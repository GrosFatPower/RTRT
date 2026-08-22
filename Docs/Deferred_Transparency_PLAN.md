# Deferred Renderer Transparency v1 Plan

## Summary

Status: implemented in the current codebase, with the per-triangle sorting follow-up described below.

Transparency in the deferred `OpenGLRasterizer` is implemented as a split pipeline:

- Keep the current deferred path for opaque geometry.
- Keep `MASK` materials in the opaque pass, with alpha-cut discard in the G-buffer shader.
- Route transparent mesh instances through a new forward transparent pass after deferred lighting.
- Support two transparent cases in v1:
  - `AlphaMode::Blend`
  - transmissive/glass-like materials where `_SpecTrans > 0.001`, even if alpha mode is still `Opaque`
- Do not implement screen-space refraction, primitive support, or order-independent transparency in v1.

This remains a conservative design aimed at mesh-instance transparency with stable behavior and explicit limitations.

## Implementation Changes

### 1. Split opaque vs transparent mesh instances

In `DeferredRenderer`, maintain two mesh-instance draw lists:

- `opaqueInstances`
- `transparentInstances`

Classification rule:

- `AlphaMode::Mask` -> opaque list
- `AlphaMode::Blend` -> transparent list
- `_SpecTrans > 0.001` -> transparent list
- everything else -> opaque list

Rebuild these lists on scene reload and on `SceneMaterials` / `SceneInstances` dirties.

Sort `transparentInstances` back-to-front every frame using camera-space depth of each instance’s transformed mesh-bounds center. Transparent meshes also have per-triangle metadata and sorted index buffers, described in the 2026-05-02 update below.

### 2. Keep the deferred pass opaque-only

Opaque rendering stays structurally the same:

- shadow map pass
- G-buffer pass
- SSAO
- deferred lighting
- final fullscreen composite

Changes inside the opaque path:

- In the deferred geometry fragment shader, load the material and perform alpha-cut discard for `MASK` materials:
  - if `AlphaMode == Mask` and effective opacity `< AlphaCutoff`, `discard`
- Do not submit `BLEND` or transmissive (`SpecTrans`) instances to the G-buffer pass.
- G-buffer contents remain opaque-only; transparent surfaces do not write depth, normals, SSAO input, or deferred debug buffers.

### 3. Add a forward transparent pass after deferred lighting

The transparent shader path:

- reuses the current deferred geometry vertex shader
- uses `Shaders/fragment_DeferredTransparent.glsl` for mesh-instance transparency

Transparent pass behavior:

- render into the existing lit scene target after deferred lighting
- attach the opaque depth texture to the lighting FBO for depth testing
- use `GL_DEPTH_TEST` with `GL_LEQUAL`
- keep depth writes disabled
- use premultiplied alpha blending with `glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA)`

Transparent fragment output policy:

- For `Blend` materials:
  - `alpha = opacity`
  - color is the transparent surface shading premultiplied by `alpha`
- For transmissive/glass-like materials:
  - `alpha = opacity * (1.0 - specTrans)`
  - diffuse/transmission tint is premultiplied by `alpha`
  - specular/direct reflection and env reflection remain additive in the premultiplied output so glass stays visible even when transmission is high
- Use existing material inputs (`Albedo`, `Opacity`, `SpecTrans`, `IOR`, `Reflectance`, `Roughness`, textures) without adding new scene-format fields
- Use the same light set, shadow maps, env map, and BRDF LUT as the deferred lighting pass
- Do not add screen-space distortion or refraction sampling in v1

This gives:
- ordinary alpha-blended transparency
- simple glass-like tint + reflection
- no physically correct refraction model

### 4. Shadowing, SSAO, debug, and UI policy

Shadowing:

- transparent/transmissive instances are skipped in shadow-map rendering
- `MASK` materials stay on the current shadow path
- do not add alpha-cut shadow maps in v1
- result: masked materials keep today’s shadow behavior; blended/transmissive materials cast no shadows in v1

SSAO:

- transparent surfaces do not contribute to SSAO generation
- transparent shading may sample the existing opaque AO result, but AO remains driven by opaque depth/normals only

Debug modes:

- `Depth`, `Normals`, `SSAO`, `Material Params`, and `Shadows` remain opaque-only views
- final `Color` output includes transparency
- wireframe debug remains opaque-only in v1

Settings/UI:

- `RenderSettings::_Transparency` defaults to `true`
- one `Transparency` checkbox exists under `OpenGLRasterizer` settings in `Test5`
- no new scene-file syntax is required

## Test Plan

Use real repo scenes for acceptance:

- `TexturedBox.scene` or another fully opaque scene: confirm no visual regression in opaque rendering.
- `tropical_island.scene` or `tropical_island2.scene`: validate `alphamode blend` materials render through the transparent pass.
- `volume_cube.scene`: validate edge behavior for very low or zero-opacity blend materials.
- `hyperion2.scene`, `hyperion.scene`, `mustang.scene`, or `renderman_teapot_teal_glass.scene`: validate `_SpecTrans` glass-like materials show background visibility plus reflection.
- A mixed scene with both opaque and transparent meshes: confirm back-to-front sorting behaves correctly while orbiting the camera.
- Shadow validation:
  - blended/transmissive meshes cast no shadows
  - masked meshes keep current shadow behavior
- Debug validation:
  - transparent meshes appear in final color
  - transparent meshes do not appear in opaque G-buffer debug views
- Runtime validation:
  - switching renderers does not break the transparent pass state
  - resizing still keeps the correct framebuffer size and camera aspect

Build acceptance:

- `cmake --build build --target RenderLab`
- runtime smoke check in `Test5` with `OpenGLRasterizer`

## Assumptions and Defaults

- v1 covers mesh instances only; primitive instances are explicitly out of scope.
- No OIT, weighted blended OIT, depth peeling, or per-pixel linked lists.
- No screen-space refraction/distortion in v1.
- No true background-tint/refraction model; glass is represented by premultiplied tint + additive reflection.
- Transparent meshes do not write depth; correctness depends on per-instance back-to-front sorting.
- Sorting is per mesh instance using transformed mesh-bounds center, not per triangle.
- `MASK` is treated as cutout-opaque, not as a transparent material.
- No scene-format changes are needed; v1 relies on existing `AlphaMode`, `Opacity`, `AlphaCutoff`, and `SpecTrans`.

## Trace Update - 2026-05-02 - v1 Per-Triangle Transparent Sorting Follow-Up

- Observed symptom:
  - Intra-mesh random triangle visibility persisted on partially transmissive glass (for example `coffee_maker.scene` with `transmission=0.3`) even with per-instance sorting.
- Confirmed root cause:
  - Single-pass transparent rendering draws triangle indices in static mesh index order.
  - Per-instance sorting only orders whole objects, not overlapping front/back triangles inside the same transparent mesh.
- Chosen fix:
  - Keep transparent/opaque classification and per-instance transparent sorting unchanged.
  - Build per-mesh transparent triangle metadata at scene reload:
    - immutable base triangle index list
    - immutable local-space triangle centers
    - reusable sorted index scratch buffers
  - In transparent rendering, for each transparent instance:
    - compute view-space depth for each triangle center
    - sort triangles back-to-front
    - rewrite the mesh EBO with sorted indices using `glBufferSubData`
    - draw once with existing premultiplied alpha blend/depth state
- Validation scenes and expected outcomes:
  - `coffee_maker.scene` (`transmission=0.3`): no random triangle mosaic on the glass pot.
  - `coffee_maker.scene` (`transmission=1.0`): no regression in glass visibility/reflection behavior.
  - `mustang.scene`: interior remains visible through windows with no regression.
  - Orbit/dolly camera in both scenes: stable ordering without flickering triangle flips.
  - Transparency OFF/ON toggle: OFF removes transparent contribution, ON uses sorted triangle rendering.
- Residual limitations:
  - Still not full OIT; intersecting transparent meshes can still show ordering conflicts.
  - Quality-first runtime policy: all transparent triangles are sorted every frame (higher CPU cost accepted for this iteration).

# Deferred Renderer Transparency v2 - Screen-Space Refraction

## Summary

Status: implemented and covered by dedicated deferred regression cases.

v2 keeps the v1 sorted forward transparency pass and adds depth-aware screen-space refraction for materials where `_SpecTrans > 0.001`. Refraction uses the current opaque HDR scene, opaque depth and position, the shaded surface normal (including normal maps), IOR, roughness, albedo, opacity, and transmission.

Ordinary `AlphaMode::Blend` materials with no transmission keep the v1 path. Disabling refraction also restores v1 transmissive shading without changing transparent classification or sorting.

## Render Pipeline and Resources

The deferred frame order is:

1. Render the opaque G-buffer, SSAO, SSR, and deferred lighting as before.
2. If transparency and refraction are enabled and at least one visible mesh uses transmission, copy the opaque lighting target into the existing SSR source texture and generate its mip chain.
3. Render sorted transparent instances and triangles into the lighting target while sampling that opaque snapshot.
4. Render the optional wireframe overlay.
5. Copy final color into the same source texture for next-frame SSR, preserving the existing SSR history behavior.

The opaque snapshot copy and mip generation are skipped for opaque scenes, refraction-disabled frames, and scenes containing only non-transmissive alpha blending. The copy has its own `Refraction source copy` GPU timing entry; ray marching remains part of `Transparency`.

The SSR source texture is time-multiplexed instead of adding another full-resolution HDR allocation. Its sampler uses trilinear mip filtering so rough refraction can sample progressively blurred scene color.

## Refraction and Composition

The transparent fragment shader models one air-to-material interface while retaining back-face culling. It computes `refract(cameraToSurface, normal, 1 / IOR)` and marches the result through world space against opaque G-buffer depth and position. The trace uses bounded fixed-size steps, opaque-depth crossing tests, four binary refinement steps, distance confidence, and screen-edge fading.

Hit behavior:

- A valid hit samples the opaque HDR snapshot at the refined UV.
- Roughness selects the source mip with `roughness * roughness * (mipCount - 1)`.
- A screen-space miss samples the refracted environment direction when an environment map is enabled.
- Without an environment map, or when confidence fades at the screen edge, sampling falls back to the undisplaced opaque scene pixel.
- IOR is clamped to at least `1.0`; IOR `1.0` therefore preserves the incident direction.

Transmissive composition uses premultiplied alpha:

- coverage alpha: `opacity`
- diffuse weight: `opacity * (1 - SpecTrans)`
- refracted background weight: `opacity * SpecTrans * (1 - Fresnel)`
- refraction tint: material albedo
- reflection: existing direct and environment specular terms using IOR-derived Schlick Fresnel behavior

The non-refraction branch remains the v1 premultiplied tint plus additive reflection model.

## Settings and UI

Defaults in `RenderSettings`:

- `_Refraction = true`
- `_RefractionMaxSteps = 48`, clamped to `4..128`
- `_RefractionStepSize = 0.18`
- `_RefractionMaxDistance = 35.0`
- `_RefractionThickness = 0.25`
- `_RefractionEdgeFade = 0.18`

The controls are exposed next to deferred transparency in Test5 and in the FPS editor. FPS map serialization uses `refraction`, `refractionMaxSteps`, `refractionStepSize`, `refractionMaxDistance`, `refractionThickness`, and `refractionEdgeFade`. Render regression manifests use the equivalent snake-case names.

Refraction is subordinate to `_Transparency`; disabling transparency skips the complete transparent pass.

## Validation

`DeferredRefraction.scene` provides a patterned opaque background, an IOR `1.0` reference pane, IOR `1.45` clear glass, rough tinted glass, an edge-of-screen glass pane, and a non-transmissive alpha-blend control.

Committed regression cases:

- `deferred_refraction`: depth-aware refraction enabled.
- `deferred_refraction_disabled`: the same scene using the v1 transmissive fallback.

Existing acceptance scenes remain `coffee_maker.scene`, `mustang.scene`, `hyperion2.scene`, and `WaterBottle.scene`, including orbit, resize, and transparency toggle checks.

## Residual Limitations

- Refraction samples only the opaque scene snapshot. Nearer glass cannot refract already-rendered farther transparent layers.
- Sorting remains per instance and per triangle; there is no OIT or intersecting-transparent-mesh solution.
- The model has no back-face exit interface, physical thickness, Beer-Lambert absorption, chromatic dispersion, or volume scattering.
- Blended and transmissive materials still cast no transparent shadows and do not contribute to SSAO or opaque G-buffer debug views.
- Primitive-instance transparency remains out of scope.
- Screen-space rays can miss off-screen or hidden geometry; confidence fading and environment/undisplaced-color fallback hide holes but cannot reconstruct unavailable scene data.
