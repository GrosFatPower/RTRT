file(GLOB_RECURSE FPS_GAME_SOURCES CONFIGURE_DEPENDS ./src/*.cpp)
file(GLOB_RECURSE FPS_GAME_HEADERS CONFIGURE_DEPENDS ./src/*.h)

set(FPS_GAME_MAIN_SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp)
list(FILTER FPS_GAME_SOURCES EXCLUDE REGEX "main\\.cpp$")

source_group("src" FILES ${FPS_GAME_SOURCES})
source_group("header" FILES ${FPS_GAME_HEADERS})

add_library(FpsGameCore STATIC
  ${FPS_GAME_SOURCES}
  ${FPS_GAME_HEADERS}
)

target_include_directories(FpsGameCore
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/Dependencies/imguizmo
)

target_compile_definitions(FpsGameCore
  PUBLIC
    GLEW_STATIC
    NOMINMAX
)

target_compile_features(FpsGameCore
  PRIVATE
    cxx_std_17
)

target_link_libraries(FpsGameCore
  PUBLIC
    RTRTFramework
    RTRTCore
    imguizmo
    imgui
    glfw
)

# Standalone Executable
add_executable(FpsGame
  ${FPS_GAME_MAIN_SOURCE}
)

target_include_directories(FpsGame
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_definitions(FpsGame
  PRIVATE
    GLEW_STATIC
    NOMINMAX
)

target_compile_features(FpsGame
  PRIVATE
    cxx_std_17
)

target_link_libraries(FpsGame
  PRIVATE
    FpsGameCore
)

set_target_properties(FpsGame PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
  RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/Debug
  RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/Release
)

if(WIN32)
  set_target_properties(FpsGame PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "$(Project)$(Configuration)"
  )
endif()

if(RTRT_HAS_OIDN AND OIDN_RUNTIME_FILES)
  add_custom_command(TARGET FpsGame POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      ${OIDN_RUNTIME_FILES}
      $<TARGET_FILE_DIR:FpsGame>
    COMMENT "Copying Intel OIDN runtime libraries to $<TARGET_FILE_DIR:FpsGame>"
  )
  if(APPLE)
    set_target_properties(FpsGame PROPERTIES
      BUILD_WITH_INSTALL_RPATH TRUE
      INSTALL_RPATH "@executable_path"
    )
  endif()
endif()
