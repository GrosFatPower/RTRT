# Test6 Prop Collision Plan

## Summary
Add opt-in collisions for imported Test6 props, starting with conservative world-space AABB collision. The first step is to save this plan in `Docs/TEST6_PROP_COLLISION_PLAN.md`. Each prop gets an editor toggle and persisted map data. Collision affects both player movement and projectile bouncing, while keeping the data model ready for a later "mesh/accurate" collision mode.

## Key Changes
- Save this plan first:
  - Add `Docs/TEST6_PROP_COLLISION_PLAN.md`.
- Extend prop map data with collision state:
  - Add `FpsPropCollisionMode { None, Bounds }`, defaulting to `None`.
  - Persist it in `.fpsmap` prop blocks as `collision none` or `collision bounds`.
  - Accept old maps by defaulting missing prop collision to `none`.
- Build prop collision bounds from loaded render data:
  - Add a lightweight `FpsPropCollisionBox` / bounds cache on `FpsGameWorld`.
  - Add a `FpsGameSceneBinding` helper that gathers each collidable prop’s visible mesh instances, transforms their mesh AABBs into world-space AABBs, and returns boxes tagged with the prop index.
  - Rebuild/update this cache after scene load, prop load, prop transform changes, prop visibility changes, prop asset reloads, and collision-mode changes.
- Apply prop collisions in gameplay:
  - Player movement uses the existing axis-separated AABB resolution path, checking scene objects first and prop collision boxes second.
  - Projectiles use the same sphere-vs-expanded-AABB bounce behavior they already use for static objects, extended to prop boxes.
  - Invisible props do not collide in v1, matching editor visibility expectations.
- Add editor UI:
  - In the prop inspector, add a collision checkbox labeled `Prop collision`.
  - Checkbox maps `false -> None`, `true -> Bounds`.
  - When enabled, selected prop collision bounds are drawn with the existing editor AABB overlay style so the blocking volume is visible.
- Keep future accurate collision clean:
  - The enum leaves room for `Mesh` or `Convex` later without replacing saved map fields.
  - The gameplay code consumes generic collision boxes now; a later accurate mode can add narrow-phase mesh checks after the broad AABB pass.

## Test Plan
- Build with `cmake --build build`.
- In Test6 editor mode, import or drop a prop, enable `Prop collision`, save, reload, and verify the setting persists.
- Verify player movement is blocked by enabled prop bounds and passes through disabled props.
- Verify projectiles bounce on enabled prop bounds and pass through disabled props.
- Move, rotate, scale, hide, show, reload, duplicate, and delete a collidable prop; verify collision bounds update immediately and do not leave stale blockers.
- Verify existing primitive box/collider collision still behaves unchanged.
- Regression-check editor mode movement remains collision-free as currently documented.

## Assumptions
- Newly imported props are non-collidable by default.
- V1 collision uses conservative world-space AABBs from each prop mesh instance, not one combined prop box, so multi-part props get better broad-phase behavior.
- Prop visibility disables collision for now.
