file(GLOB_RECURSE ENGINE_SOURCES CONFIGURE_DEPENDS ./src/*.cpp)
file(GLOB_RECURSE ENGINE_HEADERS CONFIGURE_DEPENDS ./src/*.h ./src/*.hxx)
file(GLOB_RECURSE EXT_SOURCE CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/Dependencies/RadeonRays/*.cpp)
file(GLOB_RECURSE EXT_HEADER CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/Dependencies/RadeonRays/*.h)

source_group("src" FILES ${ENGINE_SOURCES} ${EXT_SOURCE})
source_group("header" FILES ${ENGINE_HEADERS} ${EXT_HEADER})

add_library(RTRTCore STATIC
  ${ENGINE_SOURCES}
  ${ENGINE_HEADERS}
  ${EXT_SOURCE}
  ${EXT_HEADER}
)

target_include_directories(RTRTCore
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/Dependencies/glfw/include
    ${CMAKE_SOURCE_DIR}/Dependencies/glm
    ${CMAKE_SOURCE_DIR}/Dependencies/stb
    ${CMAKE_SOURCE_DIR}/Dependencies/tinyobjloader
    ${CMAKE_SOURCE_DIR}/Dependencies/tinydir
    ${CMAKE_SOURCE_DIR}/Dependencies/tinygltf
    ${CMAKE_SOURCE_DIR}/Dependencies/tinybvh
    ${CMAKE_SOURCE_DIR}/Dependencies/RadeonRays
    ${GLEW_INCLUDE_DIRS}
)

target_link_directories(RTRTCore
  PUBLIC
    ${CMAKE_SOURCE_DIR}/Dependencies/glew-2.1.0/lib/Release/x64/
)

target_compile_definitions(RTRTCore
  PUBLIC
    GLEW_STATIC
    NOMINMAX
)

if(ARCH_ARM64)
  target_compile_definitions(RTRTCore PRIVATE SIMD_ARM_NEON)
elseif(ARCH_X64)
  target_compile_definitions(RTRTCore PRIVATE SIMD_AVX2)
endif()

target_compile_features(RTRTCore
  PRIVATE
    cxx_std_17
)

if(APPLE)
  target_compile_options(RTRTCore
    PRIVATE
      -fexperimental-library
      -Wno-deprecated-declarations
  )
  if(ARCH_ARM64)
    set_target_properties(RTRTCore PROPERTIES
      OSX_ARCHITECTURES "arm64"
    )
  endif()
  target_link_libraries(RTRTCore
    PUBLIC
      ${GLEW_LIBRARIES}
      ${OPENGL_LIBRARIES}
      ${COCOA_LIBRARY}
      ${IOKIT_LIBRARY}
      ${COREVIDEO_LIBRARY}
  )
else()
  target_link_libraries(RTRTCore
    PUBLIC
      ${GLEW_LIBRARIES}
      ${OPENGL_LIBRARIES}
  )
endif()

if(RTRT_HAS_OIDN)
  target_include_directories(RTRTCore BEFORE PUBLIC ${OIDN_INCLUDE_DIR})
  target_link_libraries(RTRTCore PUBLIC ${OIDN_LIBRARY})
  target_compile_definitions(RTRTCore PUBLIC RTRT_HAS_OIDN=1)
endif()

set_target_properties(RTRTCore PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
  RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/Debug
  RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/Release
)
