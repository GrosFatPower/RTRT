# Multiple Shadow-Casting Lights Recap

Feature: multiple shadow-casting lights for the deferred `OpenGLRasterizer`

## Summary

The deferred renderer no longer selects only one shadow-casting light. Lights now carry shadow metadata, the renderer ranks eligible shadow casters up to a configurable cap, renders their shadow maps into array textures, and samples the matching shadow layer during deferred and transparent lighting.

## Behavior

- each `Light` has `_CastShadow`, defaulting to `true` for old scene compatibility
- local lights can set `_ShadowRadius`; `0` keeps the existing auto/global far-plane behavior
- `RenderSettings::_MaxShadowCastingLights` limits the selected shadow casters, clamped to the shader cap of 8
- distant lights use layers in a 2D depth texture array
- sphere and rect lights use cubemap-array layers, with six layers per selected local caster
- shadow debug mode now reports the average visibility of shadowed lights affecting the shaded pixel

## Scene And UI

Scene light blocks can optionally use:

```txt
castshadow true
shadowradius 25
```

The `Test5` light editor exposes per-light `Cast shadow` and local-light `Shadow radius` controls. Deferred renderer settings expose `Max shadow casting lights`.

## Remaining Limitations

- no cascaded directional shadows
- no temporal stabilization
- transparent objects still do not cast shadows into the shadow maps
- no OIT/refraction changes
- shadow caster ranking is a pragmatic cap, not tiled/clustered light culling
