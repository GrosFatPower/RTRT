cmake_minimum_required(VERSION 3.15)
project(RTRT
  LANGUAGES CXX
  VERSION 1.0
)

SET_PROPERTY(GLOBAL PROPERTY USE_FOLDERS ON) 

set(CMAKE_CONFIGURATION_TYPES "Debug;Release;RelWithDebInfo" CACHE STRING "Configuration types" FORCE)

option(RTRT_IMGUI_IMGUI_SHARED "Use imgui as a shared library" OFF)
option(RTRT_BUILD_IMGUI "Build imgui using sources provided in external/imgui" ON)
set(RTRT_IMGUI_SOURCE_DIR ${CMAKE_SOURCE_DIR}/Dependencies/imgui CACHE STRING "Source dir of imgui")

add_subdirectory(Source)
add_subdirectory(Dependencies)
add_subdirectory(Shaders)

set(GLEW_INCLUDE_DIRS "${CMAKE_SOURCE_DIR}/Dependencies/glew-2.1.0/include")
set(GLEW_LIBRARIES "glew32s.lib")

find_package(OpenGL REQUIRED)

file(GLOB_RECURSE CORE_SOURCE ./Source/src/*.cpp)
file(GLOB_RECURSE CORE_HEADER ./Source/src/*.h)
file(GLOB_RECURSE EXT_SOURCE ./Dependencies/RadeonRays/*.cpp)
file(GLOB_RECURSE EXT_HEADER ./Dependencies/RadeonRays/*.h)
file(GLOB_RECURSE SHADERS ./Shaders/*.glsl)

add_executable(RTRT ${CORE_SOURCE} ${CORE_HEADER} ${EXT_SOURCE} ${EXT_HEADER} ${SHADERS})

source_group("src"    FILES ${CORE_SOURCE})
source_group("header" FILES ${CORE_HEADER})
source_group("shaders" FILES ${SHADERS})

target_include_directories(RTRT
  PUBLIC ${CMAKE_SOURCE_DIR}/Dependencies/glfw/include
  PUBLIC ${CMAKE_SOURCE_DIR}/Dependencies/imgui
  PUBLIC ${CMAKE_SOURCE_DIR}/Dependencies/stb
  PUBLIC ${CMAKE_SOURCE_DIR}/Dependencies/glm
  PUBLIC ${CMAKE_SOURCE_DIR}/Dependencies/tinyobjloader
  PUBLIC ${CMAKE_SOURCE_DIR}/Dependencies/tinydir
  PUBLIC ${CMAKE_SOURCE_DIR}/Dependencies/RadeonRays
  PUBLIC ${GLEW_INCLUDE_DIRS}
)

target_link_directories(RTRT
  PUBLIC ${CMAKE_SOURCE_DIR}/Dependencies/glew-2.1.0/lib/Release/x64/
)

target_link_libraries(RTRT
  glfw
  imgui
  ${GLEW_LIBRARIES}
  ${OPENGL_LIBRARIES}
)

target_compile_definitions(RTRT
  PUBLIC GLEW_STATIC
)

target_compile_features(RTRT
 PRIVATE cxx_std_17
)

