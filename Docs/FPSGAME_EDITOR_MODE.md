# FPS Game Editor Mode Directive

## Purpose
Define an interactive editor mode for Test6 so FPS maps can be designed, inspected, modified, saved, and reloaded from inside the application. This document is a directive for building an implementation plan, not the implementation plan itself.

The editor should build on the existing Test6 game infrastructure, the shared `Scene` model, the renderer abstraction, and the `.fpsmap` map format. The first implementation should prioritize a clean, debuggable editing workflow over advanced authoring features.

## Core Experience
- The user can toggle between gameplay mode and editor mode with `F3`.
- Editor mode uses free-look first-person navigation with collisions disabled.
- The weapon model is hidden while editing.
- The crosshair remains visible and can be reused as an aiming/selection aid.
- The debug panel remains available, but editor controls should be presented in a dedicated editor section or panel.
- Existing renderer switching behavior should continue to work. The deferred renderer remains the primary target.

## Editing Capabilities
The implementation plan should cover these core editing actions:

- Select an editable scene object by clicking it in the viewport.
- Show clear selection feedback for the selected object.
- Move, rotate, and scale the selected object.
- Add primitive box objects to the map.
- Add, edit, and remove map lights.
- Add materials, edit basic material properties, and assign materials to editable instances.
- Import external visual models from `gltf`, `glb`, or `obj` files and place them in the map.
- Save the current map state.
- Load a different map while editor mode is active.

## Map And File Format Requirements
- Editor save/load must target the existing `.fpsmap` format.
- `.fpsmap` is the authoritative gameplay-authoring format for Test6 editor mode.
- Exporting to `.scene` is out of scope for the first implementation.
- Saved maps should include at least:
  - map metadata and environment map path;
  - player spawn;
  - gameplay settings relevant to Test6;
  - visible boxes;
  - invisible colliders;
  - lights;
  - imported props;
  - weapon model settings;
  - material assignments.
- Paths in saved files should remain project-relative where possible.
- The save path and load path should be explicit and visible to the user.

## Selection And Transform Requirements
- The plan should define how picking is implemented.
- Prefer reusing existing scene, camera, ray, AABB, mesh, or BVH utilities where practical.
- Selection must distinguish between visible editable objects, invisible colliders, lights, and non-editable runtime-only objects such as active projectiles.
- Transform editing must use the existing ImGuizmo dependency for viewport manipulation.
- ImGuizmo controls should support translate, rotate, and scale operations for selected editable objects.
- Transform editing should update both:
  - the live `Scene` instance data used by renderers;
  - the editor/map data model used for saving.
- Collision objects should be editable even when invisible, through an editor-only visualization or selection list.

## Runtime Boundaries
Editor mode should not turn Test6 into a full DCC tool in the first implementation. The plan should keep these boundaries clear:

- No enemy AI, scripting, animation editing, audio editing, navmesh editing, or gameplay event graph in the first drop.
- No destructive runtime mesh boolean system in the first drop.
- No requirement for path-traced interactive editing.
- Imported props are visual-only unless explicit colliders are added.
- Runtime projectiles, HUD counters, and player state are not saved as map content.

## UI Requirements
The implementation plan should include a compact editor UI with:

- editor mode status;
- current map file path;
- selected object name/type;
- transform controls for position, rotation, and scale;
- ImGuizmo operation controls for translate, rotate, and scale;
- object creation controls;
- material creation/editing controls;
- light creation/editing controls;
- load/save actions;
- optional visibility toggles for colliders, lights, and helper gizmos.

The UI should be separate from the gameplay HUD and logically separate from the general debug panel.

## Nice-To-Have Features
These should be treated as optional follow-up work unless they are cheap to include cleanly:

- Grid snapping for object movement and placement.
- A file menu for loading and saving maps.
- Duplicate/delete shortcuts.
- Primitive merging or grouping.
- Undo/redo.
- Per-object names and searchable object list.

## Plan Expectations
When building the implementation plan, include:

- a recommended first-drop scope;
- data model changes;
- renderer and scene synchronization strategy;
- input handling changes;
- picking strategy;
- map serialization strategy;
- UI layout;
- testing plan;
- limitations and follow-up tasks.

The resulting implementation should remain modular, easy to debug, and compatible with deferred, software, and path-tracer photo modes where practical.
