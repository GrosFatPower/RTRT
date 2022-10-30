add_subdirectory(src)

file(GLOB_RECURSE CORE_SOURCE ./src/*.cpp)
file(GLOB_RECURSE CORE_HEADER ./src/*.h)
file(GLOB_RECURSE SHADERS ../Shaders/*.glsl)

add_executable(RTRT ${CORE_SOURCE} ${CORE_HEADER} ${SHADERS})

source_group("src"    FILES ${CORE_SOURCE})
source_group("header" FILES ${CORE_HEADER})
source_group("shaders" FILES ${SHADERS})
