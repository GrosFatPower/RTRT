# Orthographic Camera Support Plan

Add a first-class orthographic camera mode that remains compatible with the
PathTracer, SoftwareRasterizer, and DeferredRenderer while preserving existing
perspective behavior.

## Camera and Loading

- Add a projection type and vertical orthographic view height to `Camera`.
- Route renderer, editor, gizmo, overlay, and picking projection construction
  through the active camera projection matrix.
- Support `type perspective` and `type orthographic` in `.scene` camera blocks.
  Perspective remains the default; `orthographicHeight` configures vertical
  framing for orthographic cameras.
- Load glTF orthographic cameras instead of ignoring them.

## Renderer Compatibility

- SoftwareRasterizer uses the active projection for vertex processing and
  culling, and uses a constant forward environment direction in orthographic
  mode.
- DeferredRenderer uses the active projection for geometry and passes camera
  projection data to lighting, SSR, and transparent-material shaders.
- PathTracer generates per-pixel parallel orthographic rays and disables the
  perspective depth-of-field ray path for them.
- Use the constant orthographic view direction for lighting and reflective
  shading where a perspective camera position would otherwise be used.

## Test5 and Test6

- Add a Test5 camera-panel selector with FOV controls for perspective and view
  height controls for orthographic mode.
- Keep Test5 picking, bounds overlays, and ImGuizmo synchronized with the
  selected projection.
- In Test6, expose projection controls only in editor mode, persist the
  requested projection and orthographic height in the `.fpsmap` render block,
  and restore perspective projection when gameplay resumes.

## Verification

- Add camera matrix and `.scene` parsing unit coverage.
- Add orthographic TexturedBox diagnostic render cases for Software, Deferred,
  and PathTracer.
- Generate image baselines for the three diagnostic cases on a host with a
  working OpenGL context, then convert them to normal baseline comparisons if
  stable across the target GPU configuration.
