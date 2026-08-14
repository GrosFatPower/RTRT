# Test6 Light Head Bob Plan

## Summary
Add a subtle runtime-only head bob effect for normal FPS player movement. First save this plan in `Docs/TEST6_HEAD_BOB_PLAN.md`. The bob affects both the camera and view weapon, but not the player collision capsule, editor placement helpers, projectiles, or free-look/editor navigation. The core bob logic should be modular and reusable outside Test6.

## Key Changes
- Save this plan first:
  - Add `Docs/TEST6_HEAD_BOB_PLAN.md`.
- Add a reusable head bob module:
  - Create a small standalone type such as `FpsHeadBob` / `FpsHeadBobState` with settings, phase/state, reset, update, and offset accessors.
  - Keep the update API independent of `Test6`, `Scene`, `Camera`, and renderer classes.
  - Feed it only generic movement inputs: `deltaTime`, horizontal speed, grounded, enabled, and optional movement direction basis.
- Add head bob runtime settings:
  - Store settings in the reusable module or in `FpsGameSettings` as a nested/plain struct.
  - Include enabled, amplitude, frequency, sway, and smoothing.
  - Keep these runtime-only for v1; do not persist them in `.fpsmap`.
- Integrate with `FpsGameWorld` and `FpsPlayer`:
  - Add bob state to the player/world and update it during `FpsGameWorld::Update()`.
  - Only active when `!_FreeLook`, player is grounded, and horizontal velocity is above a small threshold.
  - Smooth offset back to zero when standing still, airborne, reset, or in free-look.
  - Never modify `_Player._Position`; bob is visual only.
- Apply bob to rendering:
  - Add a helper such as `FpsPlayer::ViewPosition(settings)` or `FpsGameWorld::PlayerViewPosition(settings)`.
  - `SyncCamera()` uses the bobbed view position.
  - `BuildViewWeaponTransform()` uses the same bobbed view position so the weapon rides with the camera.
  - Editor authoring actions, prop drop placement, light placement, projectiles, and gameplay physics keep using unbobbed `EyePosition()`.
- Add runtime tuning UI:
  - Add a `Head bob` subsection in the existing F1 `Game tuning` controls.
  - Include enabled, amplitude, frequency, sway, and smoothing controls.
  - No map save/load changes in v1.

## Test Plan
- Build with `cmake --build build`.
- Run Test6 in player physics mode and verify walking produces a subtle bob.
- Sprint and verify the bob remains light and does not become disorienting.
- Stop moving, jump, fall, reset player, and toggle free-look; verify bob smoothly settles to zero.
- Verify editor mode/free-look movement has no bob.
- Verify camera and view weapon stay visually aligned during bob.
- Verify the reusable bob type has no dependencies on `Test6`, `Scene`, `Camera`, or renderer code.
- Regression-check projectiles, collisions, prop placement/drop placement, and editor gizmos are unaffected.

## Assumptions
- "Light" means subtle defaults: small amplitude, gentle smoothing, and no aggressive roll/tilt.
- Head bob is runtime tuning only for the first pass.
- The effect is visual-only and must not influence gameplay physics or authoring coordinates.
- The reusable implementation can live in FPS-facing source files for now, as long as its API is engine-agnostic.
