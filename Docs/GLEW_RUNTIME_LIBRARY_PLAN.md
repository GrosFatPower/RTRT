# GLEW Runtime Library Plan

## Goal

Remove the MSVC linker warning `LNK4098` by replacing the prebuilt static GLEW library with a GLEW static library built by this CMake project.

The warning is likely caused by linking `Dependencies/glew-2.1.0/lib/Release/x64/glew32s.lib`, which appears to use a different MSVC runtime than `RenderLab`. `RenderLab` currently builds with the DLL runtime (`/MDd` in Debug, `/MD` in Release), while the prebuilt static GLEW library likely requests the static runtime (`LIBCMT`).

## Proposed Solution

Use a source-built static GLEW dependency so GLEW inherits the same runtime-library policy as the rest of the project.

Recommended steps:

1. Add GLEW source as a dependency, preferably as a git submodule:

   ```powershell
   git submodule add https://github.com/nigels-com/glew.git Dependencies/glew-src
   git -C Dependencies/glew-src checkout glew-2.1.0
   ```

2. Set the MSVC runtime policy near the top-level CMake file before adding dependencies:

   ```cmake
   if(MSVC)
     set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
   endif()
   ```

3. Build GLEW from source on Windows:

   ```cmake
   if(WIN32)
     add_subdirectory(Dependencies/glew-src/build/cmake EXCLUDE_FROM_ALL)
     set(GLEW_INCLUDE_DIRS "${CMAKE_SOURCE_DIR}/Dependencies/glew-src/include")
     set(GLEW_LIBRARIES libglew_static)
   endif()
   ```

4. Remove the old prebuilt-library path/link setup:

   ```cmake
   set(GLEW_LIBRARIES "glew32s.lib")
   target_link_directories(${EXE_NAME}
     PUBLIC ${CMAKE_SOURCE_DIR}/Dependencies/glew-2.1.0/lib/Release/x64/
   )
   ```

5. Keep `GLEW_STATIC` for the static GLEW build:

   ```cmake
   target_compile_definitions(${EXE_NAME}
     PUBLIC GLEW_STATIC
     PUBLIC NOMINMAX
   )
   ```

## Expected Result

- Debug links against a GLEW static library built with `/MDd`.
- Release links against a GLEW static library built with `/MD`.
- `RenderLab`, `glfw`, `imgui`, `imguizmo`, and GLEW use compatible MSVC runtimes.
- The `LNK4098` warning should disappear without adding `/NODEFAULTLIB`.

## Notes

- Avoid fixing this with `/NODEFAULTLIB:LIBCMT` unless necessary. That hides the warning but can mask real runtime mismatch problems.
- If static GLEW is not required, an alternative is to switch to dynamic GLEW (`glew32.lib` plus `glew32.dll`) and remove `GLEW_STATIC`.
