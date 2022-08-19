add_subdirectory(src)

file(GLOB_RECURSE CORE_SOURCE ./src/*.cpp)
file(GLOB_RECURSE CORE_HEADER ./src/*.h)

add_executable(RTRT ${CORE_SOURCE} ${CORE_HEADER})

source_group("src"    FILES ${CORE_SOURCE})
source_group("header" FILES ${CORE_HEADER})
