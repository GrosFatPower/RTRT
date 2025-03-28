cmake_minimum_required(VERSION 3.20)
project(RTRT
  LANGUAGES CXX
  VERSION 1.0
)

SET_PROPERTY(GLOBAL PROPERTY USE_FOLDERS ON)

set(CMAKE_CONFIGURATION_TYPES "Debug;Release;RelWithDebInfo" CACHE STRING "Configuration types" FORCE)

add_subdirectory(Source)
add_subdirectory(Dependencies)
add_subdirectory(Shaders)

file(GLOB_RECURSE CORE_SOURCE ./Source/src/*.cpp)
file(GLOB_RECURSE CORE_HEADER ./Source/src/*.h)
file(GLOB_RECURSE EXT_SOURCE ./Dependencies/RadeonRays/*.cpp)
file(GLOB_RECURSE EXT_HEADER ./Dependencies/RadeonRays/*.h)
file(GLOB_RECURSE SHADERS ./Shaders/*.glsl)

source_group("src"    FILES ${CORE_SOURCE} ${EXT_SOURCE})
source_group("header" FILES ${CORE_HEADER} ${EXT_HEADER})
source_group("shaders" FILES ${SHADERS})

SET(EXE_NAME "RT_renderer")
add_executable(${EXE_NAME} ${CORE_SOURCE} ${CORE_HEADER} ${EXT_SOURCE} ${EXT_HEADER} ${SHADERS})

# IMGUI
option(RTRT_IMGUI_IMGUI_SHARED "Use imgui as a shared library" OFF)
option(RTRT_BUILD_IMGUI "Build imgui using sources provided in external/imgui" ON)
set(RTRT_IMGUI_SOURCE_DIR ${CMAKE_SOURCE_DIR}/Dependencies/imgui CACHE STRING "Source dir of imgui")

# GLEW
set(GLEW_INCLUDE_DIRS "${CMAKE_SOURCE_DIR}/Dependencies/glew-2.1.0/include")
set(GLEW_LIBRARIES "glew32s.lib")
find_package(OpenGL REQUIRED)

target_include_directories(${EXE_NAME}
  PUBLIC ${CMAKE_SOURCE_DIR}/Source/src
  PUBLIC ${CMAKE_SOURCE_DIR}/Dependencies/glfw/include
  PUBLIC ${CMAKE_SOURCE_DIR}/Dependencies/imgui
  PUBLIC ${CMAKE_SOURCE_DIR}/Dependencies/stb
  PUBLIC ${CMAKE_SOURCE_DIR}/Dependencies/glm
  PUBLIC ${CMAKE_SOURCE_DIR}/Dependencies/tinyobjloader
  PUBLIC ${CMAKE_SOURCE_DIR}/Dependencies/tinydir
  PUBLIC ${CMAKE_SOURCE_DIR}/Dependencies/tinygltf
  PUBLIC ${CMAKE_SOURCE_DIR}/Dependencies/tinybvh
  PUBLIC ${CMAKE_SOURCE_DIR}/Dependencies/RadeonRays
  PUBLIC ${GLEW_INCLUDE_DIRS}
)

target_link_directories(${EXE_NAME}
  PUBLIC ${CMAKE_SOURCE_DIR}/Dependencies/glew-2.1.0/lib/Release/x64/
)

target_link_libraries(${EXE_NAME}
  glfw
  imgui
  ${GLEW_LIBRARIES}
  ${OPENGL_LIBRARIES}
)

target_compile_definitions(${EXE_NAME}
  PUBLIC GLEW_STATIC
  PUBLIC NOMINMAX
)

target_compile_features(${EXE_NAME}
 PRIVATE cxx_std_17
)

