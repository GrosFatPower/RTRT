# Test6 Editor Docking Plan

## Summary
Implement real Dear ImGui docking for the Test6 FPS editor, using a dockspace inside the main application window. The first implementation step is to create `Docs/FPSGAME_EDITOR_PANEL_DOCKING_PLAN.md` with this plan, then update the bundled ImGui to a docking-capable version before refactoring the editor UI.

Repository inspection found that `Dependencies/imgui` is currently ImGui `1.92.0 WIP` without `IMGUI_HAS_DOCK`, `ImGuiConfigFlags_DockingEnable`, or `ImGui::DockSpace`, so true docking requires updating the bundled ImGui source.

## Key Changes
- Save this plan first:
  - Add `Docs/FPSGAME_EDITOR_PANEL_DOCKING_PLAN.md`.
- Replace `Dependencies/imgui` with the official Dear ImGui docking branch/version.
  - Keep the same local dependency layout: core files, `backends/imgui_impl_glfw.*`, and `backends/imgui_impl_opengl3.*`.
  - Keep multi-viewport disabled for v1; implement docking only inside the existing GLFW window.
  - Verify `IMGUI_HAS_DOCK` and `ImGuiConfigFlags_DockingEnable` are available after the update.
- Enable docking in Test6 UI initialization:
  - Set `io.ConfigFlags |= ImGuiConfigFlags_DockingEnable`.
  - Do not enable `ImGuiConfigFlags_ViewportsEnable`.
- Add a transparent main dockspace for editor mode:
  - Create it from `DrawUI()` before editor windows are drawn.
  - Use a central passthrough node so the existing rendered 3D viewport remains visible.
  - Keep `DrawEditorOverlays()`, `DrawEditorGizmo()`, and `DrawCrosshair()` rendering over the central viewport as they do now.
- Refactor `DrawEditorPanel()` into dockable editor windows:
  - `Editor Scene`: map status, save/load, object/prop/light lists, create buttons.
  - `Editor Inspector`: selected object/prop/light/player/weapon properties.
  - `Editor Materials`: material list and material editor.
  - `Editor Settings`: helper toggles, renderer/editor status, duplicate/delete selected, prop-list refresh.
- Initial dock layout:
  - Scene panel docked left.
  - Inspector docked right.
  - Materials and Settings docked as tabs below or beside Inspector.
  - Central area remains the interactive 3D viewport.
  - Use ImGui DockBuilder only for first-run/default layout creation; after that, respect ImGui `.ini` layout persistence.
- Keep existing behavior unchanged:
  - `F3` toggles editor mode.
  - RMB freelook, left-click picking, ImGuizmo transforms, save/load, duplicate/delete, prop asset dropdowns, and status messages continue to work.
  - Debug panel can remain a separate undocked window for now unless editor docking is active and it is explicitly moved into the dockspace later.

## Interface / Type Additions
- Add editor window visibility flags to `FpsGameEditor`, defaulting to visible:
  - `_ShowScenePanel`
  - `_ShowInspectorPanel`
  - `_ShowMaterialsPanel`
  - `_ShowSettingsPanel`
- Add dockspace helpers in `Test6`:
  - `void DrawEditorDockspace();`
  - `void DrawEditorScenePanel();`
  - `void DrawEditorInspectorPanel();`
  - `void DrawEditorMaterialsPanel();`
  - `void DrawEditorSettingsPanel();`
- Optionally add a one-frame/default-layout flag:
  - `_Editor._ResetDockLayout`
  - Expose it through a `Reset editor layout` button in `Editor Settings`.

## Test Plan
- Build with `cmake --build build`.
- Confirm the project compiles after the ImGui docking update.
- Launch Test6 and enter editor mode with `F3`.
- Verify docked panels appear in the default layout:
  - Scene left.
  - Inspector right.
  - Materials/Settings docked near Inspector.
  - 3D viewport remains visible and interactive in the center.
- Verify panels can be dragged, tabbed, resized, and redocked.
- Verify layout persists after closing/reopening the app via ImGui `.ini`.
- Verify `Reset editor layout` restores the default layout.
- Regression-check editor interactions:
  - RMB freelook still works when not over panels.
  - Left-click viewport picking still works in the central area.
  - ImGuizmo translate/rotate/scale still works.
  - Save/load status messages still appear.
  - Add/delete/duplicate objects, props, lights, and materials still work.
- Regression-check non-editor mode:
  - Gameplay UI/HUD remains unchanged when editor mode is off.
  - Debug panel still works with `F1`.

## Assumptions
- Use real ImGui docking, not manually simulated split panes.
- Docking is limited to the main application window for v1.
- The central dockspace should not render a viewport texture; it remains a passthrough area over the existing OpenGL-rendered scene.
- The initial layout is scene hierarchy on the left, inspector on the right, 3D viewport central.
