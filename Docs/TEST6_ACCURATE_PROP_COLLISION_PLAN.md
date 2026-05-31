# Accurate Prop Collision Upgrade For Test6

## Summary

Improve Test6 prop collision in two increments while keeping collision lightweight and modular:

1. Replace each collidable prop mesh instance's conservative world-space AABB with an oriented box derived from its local mesh bounds and instance transform.
2. Add optional authored compound box colliders per prop in `.fpsmap` and the FPS editor.

Retain cheap world-AABB broad-phase rejection, then perform OBB narrow-phase tests only for nearby prop colliders. Keep existing arena box/collider behavior unchanged.

Collision math must live in a dedicated helper module, not directly inside `FpsGameWorld`.

## Key Changes

### Runtime Collision

- Add a focused `FpsCollision.h/.cpp` helper module for runtime OBB data, broad-phase bounds, AABB-vs-OBB overlap, sphere-vs-OBB hits, and movement-axis correction helpers.
- Keep `FpsGameWorld` responsible for gameplay state and applying correction results; keep `FpsGameSceneBinding` responsible for building runtime collision data from scene/map data.
- Extend `FpsPropCollisionMode` with `Compound`; existing `none` and `bounds` map values remain compatible.
- Replace `FpsPropCollisionBox` with an oriented collision representation storing the prop index, world center, normalized world axes, world half extents, and a conservative world AABB for broad phase.
- Keep `Bounds` mode automatic: create one oriented collider for each visible prop mesh instance using its mesh local bounding box and render transform.
- Add OBB narrow-phase helpers for player AABB versus prop OBB using SAT, and projectile sphere versus prop OBB using closest-point testing and a collision normal.
- Preserve the current axis-stepped player movement model: reject candidates using the broad-phase AABB, resolve OBB hits along the attempted movement axis, and keep current downward-grounding behavior.
- Preserve projectile substeps; reflect velocity from OBB hits using the computed surface normal with the existing bounciness and damping settings.
- Rebuild the prop collision cache after scene load and all existing prop transform, visibility, asset, duplication, deletion, or collision-setting changes.

### Map Data And Serialization

- Add a local authored collider type owned by `FpsMapProp`, containing name, local center, local Euler rotation, and local half extents.
- Extend `.fpsmap` prop blocks with `collision compound` and nested collider blocks:

```text
prop "Table" {
  path "Furniture/table.glb"
  position 0 0 0
  rotation 0 0 0
  scale 1 1 1
  visible true
  collision compound

  collider "Top" {
    center 0 0.8 0
    rotation 0 0 0
    half 1.2 0.08 0.7
  }
}
```

- Interpret compound collider transforms in prop-local space, then apply prop position, rotation, and scale at runtime.
- For `collision compound` with no authored colliders, generate no collision and show an editor warning; do not silently fall back to render bounds.
- Continue loading existing maps unchanged: `collision bounds` automatically gains oriented-box accuracy without map migration.

### Editor Authoring

- Replace the prop collision checkbox with a mode control: `None`, `Bounds`, `Compound`.
- When switching a prop from `Bounds` to `Compound`, initialize compound colliders from currently loaded visible mesh bounds, one collider per mesh instance expressed in prop-local space.
- Add compound collider authoring beneath the selected prop inspector: list, add, duplicate, delete, rename, and local center/rotation/half-extents editing.
- Extend editor selection with a prop-collider sub-selection index so a proxy collider can be manipulated while keeping its parent prop selected.
- Draw all compound collision boxes for the selected prop using the collision-helper overlay color and highlight the active proxy.
- Use existing gizmo conventions for selected compound colliders: translate by default, rotate with Shift, and scale half extents with Alt.
- Keep prop rendering, visible-geometry picking, and scene instance ownership unchanged.

## Public Interfaces And Types

- `FpsPropCollisionMode`: add `Compound`.
- `FpsMapProp`: add `std::vector<FpsMapPropCollider> _Colliders`.
- Introduce `FpsMapPropCollider` as persisted local-space authoring data.
- Upgrade the runtime prop collision cache type to represent OBBs and broad-phase bounds.
- Add scene-binding helpers to build automatic `Bounds` OBBs, build runtime `Compound` OBBs, and generate initial editor proxy boxes when converting from `Bounds`.
- Extend `FpsEditorSelection` with a sub-index for selected compound colliders.

## Test Plan

- Build `RenderLab` Debug.
- Load existing `.fpsmap` files containing `collision none` and `collision bounds`; verify no serialization migration is required and existing collidable props still work.
- Rotate a `Bounds` prop and verify player/projectile collisions follow its oriented visible bounds instead of its oversized world AABB.
- Verify scaled and multi-mesh props create correctly positioned automatic OBBs.
- In editor mode, convert a collidable prop to `Compound`, edit generated proxies, add/delete/duplicate boxes, save, reload, and verify collider persistence and overlay/gizmo behavior.
- Verify `Compound` colliders follow prop translation, rotation, scaling, visibility, duplication, deletion, and asset reload without stale runtime collision.
- Verify empty `Compound` props do not block movement and display an editor warning.
- Verify projectiles bounce from rotated and compound collider faces with plausible normals.
- Regression-check procedural boxes/colliders, editor free-look behavior, boids, props without collision, renderer switching, and HUD behavior.

## Deferred Follow-Up Todo

1. Replace the player box with a vertical capsule and add capsule-versus-OBB movement resolution for smoother corners, slopes, and narrow obstacles.
2. Add optional BVH-backed triangle collision queries for projectiles or specially marked static props where authored boxes are insufficient; do not use render-mesh collision as the default player path.

## Assumptions

- Authored compound colliders belong to each map prop instance, not to reusable asset metadata.
- `Bounds` mode remains available and changes internally from world AABB collision to automatic mesh-instance OBB collision.
- Compound colliders use boxes only; capsule player collision and triangle/BVH collision are deliberately deferred.
- Prop visibility continues to disable its collision.
- Imported prop transforms are treated as ordinary translation/rotation/scale transforms for collision proxy generation; unsupported shear may remain conservative in `Bounds` mode.
