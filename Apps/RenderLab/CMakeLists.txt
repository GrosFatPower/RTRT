file(GLOB_RECURSE RENDERLAB_SOURCES CONFIGURE_DEPENDS ./src/*.cpp)
file(GLOB_RECURSE RENDERLAB_HEADERS CONFIGURE_DEPENDS ./src/*.h)
file(GLOB_RECURSE SHADERS CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/Shaders/*.glsl)

source_group("src" FILES ${RENDERLAB_SOURCES})
source_group("header" FILES ${RENDERLAB_HEADERS})
source_group("shaders" FILES ${SHADERS})

add_executable(RenderLab
  ${RENDERLAB_SOURCES}
  ${RENDERLAB_HEADERS}
  ${SHADERS}
)

target_include_directories(RenderLab
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/Dependencies/imguizmo
)

target_compile_definitions(RenderLab
  PRIVATE
    GLEW_STATIC
    NOMINMAX
)

target_compile_features(RenderLab
  PRIVATE
    cxx_std_17
)

target_link_libraries(RenderLab
  PRIVATE
    RTRTFramework
    RTRTCore
    imguizmo
    imgui
    glfw
)

set_target_properties(RenderLab PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
  RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/Debug
  RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/Release
)

if(WIN32)
  set_target_properties(RenderLab PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "$(Project)$(Configuration)"
  )
endif()

if(RTRT_HAS_OIDN AND OIDN_RUNTIME_FILES)
  add_custom_command(TARGET RenderLab POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      ${OIDN_RUNTIME_FILES}
      $<TARGET_FILE_DIR:RenderLab>
    COMMENT "Copying Intel OIDN runtime libraries to $<TARGET_FILE_DIR:RenderLab>"
  )
  if(APPLE)
    set_target_properties(RenderLab PROPERTIES
      BUILD_WITH_INSTALL_RPATH TRUE
      INSTALL_RPATH "@executable_path"
    )
  endif()
endif()
