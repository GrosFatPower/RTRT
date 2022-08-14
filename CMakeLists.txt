cmake_minimum_required(VERSION 3.15)
project(RTRT
  LANGUAGES CXX
  VERSION 1.0
)

add_subdirectory(Source)
add_subdirectory(Dependencies)

target_include_directories(RTRT
  PUBLIC Dependencies/glfw/include
)

target_link_libraries(RTRT
 glfw
)
