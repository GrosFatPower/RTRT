cmake_minimum_required(VERSION 3.15)
project(RTRT
  LANGUAGES CXX
  VERSION 1.0
)

option(RTRT_IMGUI_IMGUI_SHARED "Use imgui as a shared library" OFF)
option(RTRT_BUILD_IMGUI "Build imgui using sources provided in external/imgui" ON)
set(RTRT_IMGUI_SOURCE_DIR Dependencies/imgui CACHE STRING "Source dir of imgui")

add_subdirectory(Source)
add_subdirectory(Dependencies)

target_include_directories(RTRT
  PUBLIC Dependencies/glfw/include
  PUBLIC Dependencies/imgui
)

target_link_libraries(RTRT
 glfw
 imgui
 opengl32
)
