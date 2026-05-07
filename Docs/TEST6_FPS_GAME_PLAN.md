# Test6 Basic FPS Game Infrastructure Plan

## Summary
Add a sixth test as a small FPS-game sandbox built on the existing `Scene` and `Renderer` abstraction. The first increment creates reusable infrastructure: procedural arena scene, FPS player controller, collision, entity-to-scene binding, renderer switching, and a minimal debug UI. `DeferredRenderer` is the default live renderer, `SoftwareRasterizer` is an optional live renderer, and `PathTracer` is exposed only as a paused static photo mode.

## Key Changes
- Register `Test6` in app selection, CLI parsing, usage text, and the ImGui test selection panel.
- Add game-facing modules for settings, player state, world update/collision, scene binding, and procedural meshes.
- Add explicit free-look camera pose support while preserving existing orbit and free-look behavior.
- Build a simple procedural arena from reusable cube meshes, simple materials, and a few lights.
- Implement FPS input with mouse capture, WASD movement, jump, sprint, reset, and UI-safe capture release.
- Use Deferred as the default live renderer, Software as a live option, and PathTracer as paused static photo mode.
- Add compact Test6 UI for renderer mode, player stats, movement/collision tuning, reset, and a small render-settings subset.

## Implementation Notes
- Keep `Test5` behavior unchanged.
- Use existing renderer dirty states: `SceneCamera` for player camera movement and `SceneInstances` for game transform updates.
- Avoid adding/removing scene instances during gameplay after initialization.
- Keep v1 gameplay simple: static level blocks, static target crates, and the player camera.
- Clamp large frame deltas to keep collision stable after stalls or renderer switches.

## Test Plan
- Build with `cmake --build build`.
- Launch `Test6` from CLI and selection panel.
- Verify deferred startup, procedural arena visibility, resize, cursor capture/release, mouse-look, WASD, jump, sprint, reset, and collision.
- Switch to Software renderer and verify the same scene remains playable.
- Enter PathTracer photo mode and verify gameplay pauses, the view is static, accumulation works, and leaving photo mode resumes live rendering.
- Verify render-to-file still works from all Test6 renderer modes.
- Confirm Test1-Test5 still launch and Test5 behavior is unchanged.

## Assumptions
- V1 is a renderer/gameplay integration scaffold, not a complete game.
- Deferred renderer is the primary real-time target.
- Software renderer support means functional and debuggable rather than high-framerate with large scenes.
- PathTracer support means static photo rendering only.
- Procedural geometry is enough for the first increment.
