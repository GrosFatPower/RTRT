# Plan: Deferred Renderer Transparency v1

## Summary

Implement transparency in the deferred `OpenGLRasterizer` as a split pipeline:

- Keep the current deferred path for opaque geometry.
- Keep `MASK` materials in the opaque pass, with alpha-cut discard in the G-buffer shader.
- Route transparent mesh instances through a new forward transparent pass after deferred lighting.
- Support two transparent cases in v1:
  - `AlphaMode::Blend`
  - transmissive/glass-like materials where `_SpecTrans > 0.001`, even if alpha mode is still `Opaque`
- Do not implement screen-space refraction, primitive support, or order-independent transparency in v1.

This is a conservative, shippable design aimed at mesh-instance transparency with stable behavior and explicit limitations.

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

Sort `transparentInstances` back-to-front every frame using camera-space depth of each instance’s transformed mesh-bounds center. Do not use object insertion order or mesh ID order.

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

Add one new transparent shader path:

- reuse the current deferred geometry vertex shader
- add a new transparent fragment shader for mesh-instance transparency

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

- add `RenderSettings::_Transparency` default `true`
- add one `Transparency` checkbox under `OpenGLRasterizer` settings in `Test5`
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
