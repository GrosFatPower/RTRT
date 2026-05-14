# Test6 FPS Map Editor Mode Development Plan

## Summary
Add a Test6 editor mode toggled with `F3`, backed by `.fpsmap` as the only authoring/save format. The editor uses visible-cursor navigation with right-mouse freelook, left-click selection, ImGuizmo transforms, dedicated editor UI, and live synchronization with the existing `Scene`, `FpsGameWorld`, and renderer dirty-state system.

## Key Interfaces And Format Changes
- Extend the `.fpsmap` model with editable material data:
  - add `FpsMapMaterial { _Name, _Material }`;
  - add `std::vector<FpsMapMaterial> _Materials` to `FpsGameMap`;
  - replace hardcoded object material storage with a material name string while preserving existing names: `floor`, `wall`, `pillar`, `crate`, `accent`.
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
    - boxes/colliders: translate and scale only, preserving axis-aligned collision correctness;
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
  - translate and scale boxes/colliders;
  - translate/rotate/scale props;
  - translate local lights;
  - confirm live render updates and collisions match edited axis-aligned boxes.
- Verify authoring:
  - add a box, collider, material, sphere light, distant light, GLTF/GLB prop, and OBJ prop;
  - assign materials to boxes and OBJ props;
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
- Primitive boxes and colliders remain axis-aligned in v1; rotated colliders are follow-up work.
- Imported props are visual-only unless paired with explicit colliders.
- Runtime projectiles, player transient state, and HUD counters are not map content.
- ImGuizmo is mandatory for viewport transforms, with type-specific operation support to keep collision behavior correct.
