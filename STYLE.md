# Coding Style

This document records the dominant coding conventions used across the handwritten source files in this repository.

## Naming

- Types use `PascalCase`: `Camera`, `RenderSettings`, `ShaderProgram`.
- Functions use `camelCase`: `InitializeScene`, `RenderToScreen`, `ComputeFrustum`.
- Data members use a leading underscore: `_Scene`, `_FrameNum`, `_Forward`.
- Function parameters use the established prefix convention:
  `iFoo` input, `oFoo` output, `ioFoo` input/output.

## File Organization

- Prefer one header and one source file per class or subsystem.
- Use include guards with the project's existing format, for example `_Camera_`.
- Keep implementation files structured with explicit section banners such as:

```cpp
// ----------------------------------------------------------------------------
// CTOR
// ----------------------------------------------------------------------------
```

## Formatting

- Keep a space inside parentheses and after control keywords:
  `if ( condition )`
- Keep a space before opening braces.
- Use compact inline getters/setters when the body is trivial.
- Favor readable vertical layout over compressed one-liners for real logic.

## Coding Approach

- Prefer explicit, imperative code that is easy to trace in a debugger.
- Favor straightforward loops and state updates over highly abstract or functional-style rewrites.
- Keep the rendering pipeline visually understandable in code: initialization, scene update, uniform update, render passes, presentation.
- Use comments sparingly and only where they add intent, stage labeling, or technical clarification.

## Ownership And Resources

- Match the local style of the file you are editing.
- Older code may use raw pointers with explicit cleanup.
- Newer code may use `std::unique_ptr` or `std::shared_ptr` where that already fits the surrounding code.
- OpenGL resources are usually managed explicitly and visibly.

## Data Types And Engine Conventions

- Use the project math aliases from `MathUtil.h`: `Vec2`, `Vec3`, `Vec4`, `Mat4x4`, and related integer variants.
- Engine-style containers and settings types often remain plain structs with public fields.
- Preserve the existing public-field style for types like `Material`, `RenderSettings`, `GLTexture`, and similar data carriers unless there is a strong reason not to.

## Refactoring Guidance

- Preserve historical continuity across the codebase.
- Do not modernize for its own sake.
- When making changes, prefer aligning with nearby code over introducing a new style.
- Introduce broader architectural cleanup only when it clearly supports the current design and the user wants that change.
