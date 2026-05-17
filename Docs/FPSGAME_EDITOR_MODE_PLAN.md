# Test6 FPS Map Editor Mode Development Plan

## Summary
Add a Test6 editor mode toggled with `F3`, backed by `.fpsmap` as the only authoring/save format. The editor uses visible-cursor navigation with right-mouse freelook, left-click selection, ImGuizmo transforms, dedicated editor UI, and live synchronization with the existing `Scene`, `FpsGameWorld`, and renderer dirty-state system.

## Current Status
This section records the implementation status after the first editor-mode passes.

### Implemented
- Test6 now stores the active `FpsGameMap`, map path, and load status instead of loading into a short-lived local map.
- `.fpsmap` material support exists:
  - `FpsMapMaterial { _Name, _Material }`;
  - `FpsGameMap::_Materials`;
  - default material seeding for `floor`, `wall`, `pillar`, `crate`, and `accent`;
  - material block parsing for albedo, roughness, metallic, reflectance, emission, opacity, and alpha mode;
  - deterministic `FpsGameMapLoader::Save`.
- Map objects now carry `_MaterialName` while retaining the old `FpsMaterialSlot` fallback for compatibility.
- Test6 scene binding resolves map object material names into `Scene` material IDs.
- `F3` toggles editor mode:
  - entering editor mode releases cursor capture, hides the view weapon, clears active projectiles, disables firing/gameplay update, and enables editor freelook;
  - exiting editor mode restores the previous view-weapon and free-look state.
- Editor-mode navigation works with visible cursor and RMB-held freelook using WASD, Space, Ctrl, and Shift.
- Test6 has a dedicated editor panel separate from the HUD and debug panel.
- Editor UI currently supports:
  - `.fpsmap` Save and Load path fields;
  - ImGui list boxes for instances, materials, and lights;
  - add box and add collider;
  - edit object name, center, rotation, half extents, collidable flag, material assignment, and runtime instance visibility;
  - add `gltf`, `glb`, and `obj` props;
  - list props and edit prop name, path, position, rotation, scale, and visibility;
  - add material and edit material parameters;
  - add sphere light and distant light;
  - edit light type, position/direction, emission, intensity, radius/area, shadow casting, and shadow radius;
  - edit player spawn and copy current editor camera pose into the spawn.
- ImGuizmo is integrated into the Test6 UI frame:
  - selected boxes/colliders can be translated in world space;
  - holding `Shift` switches selected box/collider gizmos from translate to rotate mode, releasing `Shift` returns to translate mode;
  - holding `Alt`/Option switches selected box/collider gizmos from translate to scale mode, releasing `Alt` returns to translate mode;
  - selected props can be translated, Shift-rotated, and Alt-scaled in world space;
  - selected local lights can be translated in world space;
  - distant lights remain direction-edited through UI fields.
- Map objects now carry `_Rotation` for UI/legacy editing and `_Orientation` for stable authored orientation. `.fpsmap` box/collider blocks support `rotation x y z` and `orientation x y z w`, and save writes both fields.
- Rotated box/collider rendering, picking, and selection overlays use the same transform; runtime collision remains AABB-based by using the rotated object's world-space bounds.
- Viewport picking is implemented for editor mode:
  - visible boxes can be selected with left click;
  - colliders can be selected with left click while collider helpers are enabled;
  - props can be selected with left click using mesh triangle tests;
  - local lights can be selected with left click while light helpers are enabled;
  - projectiles and the view weapon are ignored because picking is performed against editor map objects/lights.
- Selection feedback overlays are implemented:
  - selected box/collider bounds are drawn in the foreground overlay;
  - selected prop mesh bounds are drawn in the foreground overlay;
  - collider helper wire boxes can be toggled;
  - local light helper markers can be toggled;
  - selected local lights remain visible even when light helpers are hidden.
- Editor scene binding tracks prop index to generated scene mesh instance IDs and base transforms. Transform-only prop edits live-sync without rebuilding.
- Editor-triggered scene rebuilds preserve the current editor camera/player pose.
- `FpsGameSceneBinding::Attach` now performs initial camera and transform sync so hidden weapon/projectile instances are valid immediately after rebuild.
- `MeshInstance` has a `_Visible` flag.
- Deferred, Software, PathTracer/TLAS, scene compilation, Test5 picking/bounds, and Test6 runtime binding respect mesh-instance visibility.
- Test6 runtime now hides inactive projectiles and the view weapon through `MeshInstance::_Visible` instead of moving/scaling them offscreen.
- Test6 editor can temporarily hide visible map instances without changing the `.fpsmap` box/collider meaning.

### Partially Implemented
- Editor scene binding tracks object-to-scene-instance IDs for boxes through `_ObjectInstanceIDs`, but this is still internal to `FpsGameSceneBinding`.
- Light map index and `Scene` light index currently match because Test6 adds map lights in order and editor light edits sync by index. This should become an explicit binding if default/fallback lights, helper lights, or imported props add more light sources.
- Transform-only object and light edits live-sync without rebuilding.
- Adding boxes, colliders, props, lights, or materials currently triggers rebuilds where needed, but rebuild policy is still coarse for some cases.
- Runtime instance visibility is editor-state only; it is not saved to `.fpsmap`.

### Not Yet Implemented
- OBJ prop material assignment; OBJ props currently use the default wall material.
- Delete, duplicate, undo/redo, object search, and object renaming polish beyond the current name field.
- Persistent saved per-object editor visibility.
- In-editor validation/status messages for save/load success or failure.

### Known Design Notes
- `FpsSceneObject::_Visible` still means map-visible box versus invisible collider. It should not be reused for temporary editor hiding, because that would change saved map semantics.
- Temporary editor hiding uses `MeshInstance::_Visible` through Test6 editor state.
- Hidden mesh instances must stay out of scene bounds, shadow fitting, software-rasterizer caches, and TLAS data. This is now handled by the shared `_Visible` flag.
- Distant-light shadow quality previously degraded in editor mode because hidden weapon geometry was moved far below the arena and inflated scene bounds. The visibility flag removes that class of workaround.
- Rotated primitive collision uses the rotated object's world-space AABB for now. This keeps gameplay conservative without adding full OBB collision resolution yet.

### Recommended Next Slice
Add delete/duplicate actions and save/load status messages, then add OBJ prop material assignment.

## Key Interfaces And Format Changes
- Extend the `.fpsmap` model with editable material data:
  - add `FpsMapMaterial { _Name, _Material }`;
  - add `std::vector<FpsMapMaterial> _Materials` to `FpsGameMap`;
  - replace hardcoded object material storage with a material name string while preserving existing names: `floor`, `wall`, `pillar`, `crate`, `accent`.
- Extend map objects with rotation:
  - add `Vec3 _Rotation` to `FpsSceneObject`;
  - add `Vec4 _Orientation` to store the stable quaternion used by runtime/editor transforms;
  - support optional `rotation x y z` in `.fpsmap` box/collider blocks;
  - support optional `orientation x y z w` in `.fpsmap` box/collider blocks;
  - save `rotation` and `orientation` deterministically for authored objects.
- Add `.fpsmap` serialization:
  - keep `FpsGameMapLoader::Load`;
  - add a matching deterministic `Save` API, or introduce `FpsGameMapIO::Load/Save` if renaming is cleaner;
  - write project-relative asset paths whenever possible.
- Extend `.fpsmap` syntax with material blocks:
  ```txt
  material "mat_name" {
    albedo 1 1 1
    roughness 0.5
    metallic 0
    reflectance 0.5
    emission 0 0 0
    opacity 1
    alphamode opaque
  }
  ```
- Keep existing maps valid:
  - maps without material blocks use built-in default materials;
  - saving from the editor writes all material names currently used by editable objects.
- Add editor state types:
  - `FpsEditableKind`: none, box, collider, prop, light, player spawn, weapon;
  - `FpsEditorSelection`: selected kind, map index, scene instance id when applicable;
  - `FpsGameEditor`: owns editor mode state, selected object, gizmo operation/mode/snap settings, helper visibility flags, current map path, and dirty/save status.

## Implementation Tasks
- Save this plan:
  - create `Docs/FPSGAME_EDITOR_MODE_PLAN.md` containing the final approved plan before implementation starts.
- Refactor map-backed runtime state:
  - store the currently loaded `FpsGameMap` and map path in Test6/editor state;
  - initialize Test6 from that map instead of using a short-lived local map;
  - preserve fallback arena behavior if initial map load fails.
- Add map material support:
  - seed default materials for `floor`, `wall`, `pillar`, `crate`, and `accent`;
  - resolve object material names through the editor material registry;
  - expose basic material editing for albedo, roughness, metallic, reflectance, emission, opacity, and alpha mode;
  - notify `DirtyState::SceneMaterials` after material edits.
- Add editor scene binding support:
  - track map object index to scene instance id for boxes;
  - track prop index to one or more scene instance ids;
  - track map light index to `Scene` light index;
  - expose helpers to sync one edited object, prop, or light without rebuilding the whole scene when only transforms/properties changed.
- Define rebuild policy:
  - transform-only changes notify `SceneInstances`;
  - material parameter changes notify `SceneMaterials`;
  - light changes notify `SceneLights`;
  - adding/removing boxes, materials, props, or imported model resources rebuilds the Test6 scene and recreates the active renderer to keep GPU buffers coherent.
- Implement editor mode input:
  - `F3` toggles editor mode;
  - entering editor mode releases mouse capture, hides the weapon, disables firing, disables gameplay collision, and clears active projectiles;
  - exiting editor mode restores gameplay input and weapon visibility;
  - right mouse held enables freelook camera movement with existing WASD, Space, and Ctrl free-look controls;
  - left click selects or manipulates editor objects when ImGui does not capture mouse;
  - `F1`, `J`, `K`, and `L` keep their current behavior.
- Reuse Test5 picking and gizmo patterns:
  - adapt `BuildPickingRay` to Test6;
  - pick visible boxes and props using mesh triangle tests;
  - pick invisible colliders using ray-AABB tests when collider helpers are visible or via the object list;
  - pick local lights using a small editor-only ray sphere test;
  - ignore runtime projectiles and the view weapon during picking.
- Add selection feedback:
  - draw selected object bounds in an ImGui foreground overlay;
  - draw collider wire boxes when collider helpers are enabled or selected;
  - draw light helper markers with foreground overlays, not mesh instances, so they never cast shadows.
- Add ImGuizmo transforms:
  - call `ImGuizmo::BeginFrame()` from Test6 UI rendering;
  - use ImGuizmo for mandatory viewport manipulation;
  - operation availability is type-specific in v1:
    - boxes/colliders: translate, Shift-held rotate, and Alt-held scale;
    - props: translate, rotate, and scale;
    - local lights: translate;
    - distant lights: direction edited through UI fields;
    - player spawn and weapon pose: edited through UI fields first, gizmo optional later.
  - after manipulation, update both the live `Scene` and the backing `FpsGameMap`.
- Add editor UI:
  - create a dedicated editor panel separate from HUD and general debug settings;
  - show editor mode status, current `.fpsmap` path, unsaved marker, selected object kind/name, and transform fields;
  - provide operation controls for translate/rotate/scale, local/world mode where supported, and optional snap values;
  - provide creation buttons for box, collider, sphere light, distant light, and material;
  - provide prop import fields for `gltf`, `glb`, and `obj` paths;
  - for OBJ props, create a single mesh instance using `Scene::AddMesh` and require or default a material name;
  - for GLTF/GLB props, use existing scene loading and record all generated mesh instances under one map prop;
  - provide Save, Save As path field, and Load path field.
- Implement save/load behavior:
  - Save writes the current editor map to the selected `.fpsmap`;
  - Load parses into a temporary map first; on success, replace the editor map, rebuild world/scene/renderer, and reset selection;
  - on load failure, keep the current map and scene unchanged and print parser diagnostics;
  - runtime-only state such as projectiles, current health after damage, HUD counters, and frame/debug state is never saved.
- Preserve renderer compatibility:
  - Deferred remains the default and primary editor target;
  - Software renderer should display the same edited scene after rebuild/sync;
  - Photo PathTracer can render the current static editor scene but does not support interactive editing while accumulating.

## Test Plan
- Build with `cmake --build build`.
- Launch Test6 and verify existing gameplay still works with editor mode off.
- Toggle editor mode with `F3`:
  - cursor releases;
  - weapon hides;
  - projectiles stop firing;
  - RMB freelook movement works without collisions;
  - exiting editor restores gameplay behavior.
- Verify selection:
  - select visible boxes and props by left click;
  - select local lights by helper marker;
  - select invisible colliders through helper visualization or object list;
  - confirm projectiles and weapon cannot be selected.
- Verify ImGuizmo:
  - translate, Shift-rotate, and Alt-scale boxes/colliders;
  - translate/rotate/scale props;
  - translate local lights;
  - confirm live render updates and gameplay collision uses the rotated primitive's conservative world-space AABB.
- Verify authoring:
  - add a box, collider, material, sphere light, distant light, GLTF/GLB prop, and OBJ prop;
  - assign materials to boxes;
  - verify OBJ prop material assignment once that follow-up is implemented;
  - edit material and light parameters.
- Verify save/load:
  - save to `.fpsmap`;
  - restart or reload the map and confirm geometry, colliders, lights, materials, environment, player spawn, props, and weapon settings persist;
  - intentionally load an invalid map and confirm the current scene remains unchanged.
- Verify renderer modes:
  - Deferred reflects edits immediately after sync/rebuild;
  - Software renders the edited scene;
  - Photo PathTracer renders a static edited scene after renderer switch.
- Regression check Test1-Test5 launch and Test5 ImGuizmo behavior is unchanged.

## Assumptions And Defaults
- `.fpsmap` is the only editor save/load target; `.scene` export is out of scope.
- Native file dialogs are out of scope for v1; paths are edited through ImGui text fields.
- Primitive boxes and colliders can be visually rotated in v1; their gameplay collision currently uses the rotated primitive's conservative world-space AABB, not full OBB collision.
- Imported props are visual-only unless paired with explicit colliders.
- Runtime projectiles, player transient state, and HUD counters are not map content.
- ImGuizmo is mandatory for viewport transforms, with type-specific operation support to keep collision behavior correct.
