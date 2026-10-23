file(GLOB TEST_SOURCE CONFIGURE_DEPENDS ./*.cpp)
file(GLOB TEST_HEADER CONFIGURE_DEPENDS ./*.h)

source_group("tests" FILES ${TEST_SOURCE} ${TEST_HEADER})

add_executable(RenderRegression
  ${TEST_SOURCE}
  ${TEST_HEADER}
)

target_include_directories(RenderRegression
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_definitions(RenderRegression
  PRIVATE
    GLEW_STATIC
    NOMINMAX
)

target_compile_features(RenderRegression
  PRIVATE
    cxx_std_17
)

target_link_libraries(RenderRegression
  PRIVATE
    RTRTCore
    nlohmann_json::nlohmann_json
    glfw
)

set_target_properties(RenderRegression PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
  RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/Debug
  RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/Release
)

if(WIN32)
  set_target_properties(RenderRegression PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "$(Project)$(Configuration)"
  )
endif()

if(RTRT_HAS_OIDN AND OIDN_RUNTIME_FILES)
  add_custom_command(TARGET RenderRegression POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      ${OIDN_RUNTIME_FILES}
      $<TARGET_FILE_DIR:RenderRegression>
    COMMENT "Copying Intel OIDN runtime libraries to $<TARGET_FILE_DIR:RenderRegression>"
  )
  if(APPLE)
    set_target_properties(RenderRegression PROPERTIES
      BUILD_WITH_INSTALL_RPATH TRUE
      INSTALL_RPATH "@executable_path"
    )
  endif()
endif()

add_custom_target(RenderWhiteFurnace
  COMMAND $<TARGET_FILE:RenderRegression> --manifest ${CMAKE_CURRENT_SOURCE_DIR}/RenderTests.WhiteFurnace.json --all
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  DEPENDS RenderRegression
  COMMENT "Running white furnace energy-conservation suite"
)

find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_FOUND)
  add_custom_target(GenerateBlenderReferences
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/Tools/render_test_reference.py --all --compare
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Generating Blender Cycles ground truth references for regression tests"
  )
endif()

if(BUILD_TESTING)
  set(RENDER_TEST_MANIFEST ${CMAKE_CURRENT_SOURCE_DIR}/RenderTests.json)
  set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${RENDER_TEST_MANIFEST})
  file(READ ${RENDER_TEST_MANIFEST} RENDER_TEST_MANIFEST_JSON)
  string(JSON RENDER_TEST_COUNT LENGTH ${RENDER_TEST_MANIFEST_JSON} tests)

  add_test(NAME UnitImageComparison COMMAND RenderRegression --unit)
  set_tests_properties(UnitImageComparison PROPERTIES
    LABELS "unit"
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  )

  math(EXPR RENDER_TEST_LAST_INDEX "${RENDER_TEST_COUNT} - 1")
  foreach(RENDER_TEST_INDEX RANGE ${RENDER_TEST_LAST_INDEX})
    string(JSON RENDER_CASE GET ${RENDER_TEST_MANIFEST_JSON} tests ${RENDER_TEST_INDEX} name)
    string(JSON RENDER_PROFILE GET ${RENDER_TEST_MANIFEST_JSON} tests ${RENDER_TEST_INDEX} profile)
    string(JSON RENDER_BACKEND GET ${RENDER_TEST_MANIFEST_JSON} profiles ${RENDER_PROFILE} backend)

    add_test(NAME Render_${RENDER_CASE} COMMAND RenderRegression --case ${RENDER_CASE})
    set_tests_properties(Render_${RENDER_CASE} PROPERTIES
      LABELS "render;${RENDER_BACKEND}"
      SKIP_RETURN_CODE 77
      WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
  endforeach()
endif()
