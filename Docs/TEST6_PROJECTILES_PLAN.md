# Test6 Bouncing Projectiles Plan

## Summary
Add left-click shooting to Test6 as a pooled gameplay feature: small red sphere projectiles spawn from the FPS camera, fly forward, bounce against the static arena AABBs, and render through the same `Scene` mesh-instance path used by Deferred, Software, and PathTracer photo mode. V1 is bounce-only: no crate movement, damage, enemies, decals, particles, or audio.

## Key Changes
- Extend Test6 gameplay settings and input with projectile radius, speed, gravity, bounciness, lifetime, pool size, cooldown, and a captured-left-click fire action.
- Add pooled `FpsProjectile` state in `FpsGameWorld`, reusing inactive slots and overwriting the oldest active projectile when the pool is full.
- Add `ProceduralMesh::CreateUVSphere` for renderer-compatible red ball mesh instances.
- Bind projectiles as preallocated mesh instances in `FpsGameSceneBinding`, updating transforms for active balls and hiding inactive ones.

## Implementation Notes
- First left click captures the mouse; subsequent left clicks while captured fire. Photo PathTracer mode ignores firing and pauses projectile simulation.
- Projectiles spawn from the player eye position along the pitch-aware camera forward direction.
- Projectile simulation uses clamped/substepped movement, gravity, sphere-vs-expanded-AABB collision, axis-separated resolution, velocity reflection, bounciness, and mild tangential damping.
- Deferred and Software renderers receive ordinary `SceneInstances` updates. PathTracer photo mode freezes the current scene state until leaving photo mode.

## Test Checklist
- Build with `cmake --build build`.
- Verify first click captures without firing, captured left click fires, red balls spawn from the camera center, travel with pitch-aware aim, bounce on all static arena objects, expire, and reuse slots.
- Verify reset/clear removes active projectiles.
- Verify Deferred, Software, and Photo PathTracer modes render the same projectile positions appropriately.
- Regression check WASD, mouse look, jump, SSAO debug views, image capture, and Test1-Test5 launch behavior.
