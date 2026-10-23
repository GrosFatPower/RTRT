file(GLOB_RECURSE FRAMEWORK_SOURCES CONFIGURE_DEPENDS ./src/*.cpp)
file(GLOB_RECURSE FRAMEWORK_HEADERS CONFIGURE_DEPENDS ./src/*.h)

source_group("src" FILES ${FRAMEWORK_SOURCES})
source_group("header" FILES ${FRAMEWORK_HEADERS})

add_library(RTRTFramework STATIC
  ${FRAMEWORK_SOURCES}
  ${FRAMEWORK_HEADERS}
)

target_include_directories(RTRTFramework
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/Dependencies/imgui
    ${CMAKE_SOURCE_DIR}/Dependencies/imgui/backends
    ${CMAKE_SOURCE_DIR}/Dependencies/nativefiledialog/src/include
)

target_compile_definitions(RTRTFramework
  PUBLIC
    GLEW_STATIC
    NOMINMAX
)

target_compile_features(RTRTFramework
  PRIVATE
    cxx_std_17
)

target_link_libraries(RTRTFramework
  PUBLIC
    RTRTCore
    imgui
    nativefiledialog
    glfw
)
