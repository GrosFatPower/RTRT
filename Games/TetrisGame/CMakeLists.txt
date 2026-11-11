file(GLOB_RECURSE TETRIS_GAME_SOURCES CONFIGURE_DEPENDS ./src/*.cpp)
file(GLOB_RECURSE TETRIS_GAME_HEADERS CONFIGURE_DEPENDS ./src/*.h)

set(TETRIS_GAME_MAIN_SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp)
list(FILTER TETRIS_GAME_SOURCES EXCLUDE REGEX "main\\.cpp$")

source_group("src" FILES ${TETRIS_GAME_SOURCES})
source_group("header" FILES ${TETRIS_GAME_HEADERS})

add_library(TetrisGameCore STATIC
  ${TETRIS_GAME_SOURCES}
  ${TETRIS_GAME_HEADERS}
)

target_include_directories(TetrisGameCore
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/Dependencies/imguizmo
)

target_compile_definitions(TetrisGameCore
  PUBLIC
    GLEW_STATIC
    NOMINMAX
)

target_compile_features(TetrisGameCore
  PRIVATE
    cxx_std_17
)

target_link_libraries(TetrisGameCore
  PUBLIC
    RTRTFramework
    RTRTCore
    imgui
    glfw
)

# Standalone Executable
add_executable(TetrisGame
  ${TETRIS_GAME_MAIN_SOURCE}
)

target_include_directories(TetrisGame
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_definitions(TetrisGame
  PRIVATE
    GLEW_STATIC
    NOMINMAX
)

target_compile_features(TetrisGame
  PRIVATE
    cxx_std_17
)

target_link_libraries(TetrisGame
  PRIVATE
    TetrisGameCore
)

set_target_properties(TetrisGame PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
  RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/Debug
  RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/Release
)

if(WIN32)
  set_target_properties(TetrisGame PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "$(Project)$(Configuration)"
  )
  # Hide console window in Release builds while preserving standard main() entry point
  target_link_options(TetrisGame PRIVATE
    "$<$<CONFIG:Release>:/SUBSYSTEM:WINDOWS;/ENTRY:mainCRTStartup>"
  )
endif()

if(RTRT_HAS_OIDN AND OIDN_RUNTIME_FILES)
  add_custom_command(TARGET TetrisGame POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      ${OIDN_RUNTIME_FILES}
      $<TARGET_FILE_DIR:TetrisGame>
    COMMENT "Copying Intel OIDN runtime libraries to $<TARGET_FILE_DIR:TetrisGame>"
  )
  if(APPLE)
    set_target_properties(TetrisGame PROPERTIES
      BUILD_WITH_INSTALL_RPATH TRUE
      INSTALL_RPATH "@executable_path"
    )
  endif()
endif()

if(BUILD_TESTING)
  add_test(NAME UnitTetrisGuideline COMMAND TetrisGame --unit-tests)
  set_tests_properties(UnitTetrisGuideline PROPERTIES
    LABELS "unit;tetris"
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  )
endif()

# -----------------------------------------------------------------------------
# Installation & Packaging rules (CPack Component: TetrisGame)
# -----------------------------------------------------------------------------
install(TARGETS TetrisGame
  RUNTIME DESTINATION .
  COMPONENT TetrisGame
)

if(RTRT_HAS_OIDN AND OIDN_RUNTIME_FILES)
  install(FILES ${OIDN_RUNTIME_FILES}
    DESTINATION .
    COMPONENT TetrisGame
  )
endif()

# Shaders
install(DIRECTORY ${CMAKE_SOURCE_DIR}/Shaders
  DESTINATION .
  COMPONENT TetrisGame
  PATTERN "*.tmp" EXCLUDE
  PATTERN ".DS_Store" EXCLUDE
)

# Packaged Background HDRs (6 curated skyboxes)
install(FILES
  ${CMAKE_SOURCE_DIR}/Assets/HDR/syferfontein_18d_clear_1k.hdr
  ${CMAKE_SOURCE_DIR}/Assets/HDR/alps_field_2k.hdr
  ${CMAKE_SOURCE_DIR}/Assets/HDR/konzerthaus_4k.hdr
  ${CMAKE_SOURCE_DIR}/Assets/HDR/peppermint_powerplant_4k.hdr
  ${CMAKE_SOURCE_DIR}/Assets/HDR/sunset.hdr
  ${CMAKE_SOURCE_DIR}/Assets/HDR/artist_workshop_1k.hdr
  DESTINATION Assets/HDR
  COMPONENT TetrisGame
)

# UI Fonts
install(FILES
  ${CMAKE_SOURCE_DIR}/Dependencies/imgui/misc/fonts/Roboto-Medium.ttf
  ${CMAKE_SOURCE_DIR}/Dependencies/imgui/misc/fonts/Cousine-Regular.ttf
  DESTINATION Assets/Fonts
  COMPONENT TetrisGame
)
install(FILES
  ${CMAKE_SOURCE_DIR}/Dependencies/imgui/misc/fonts/Roboto-Medium.ttf
  ${CMAKE_SOURCE_DIR}/Dependencies/imgui/misc/fonts/Cousine-Regular.ttf
  DESTINATION Dependencies/imgui/misc/fonts
  COMPONENT TetrisGame
)

# Resources directory for backward compatibility
install(DIRECTORY DESTINATION Resources COMPONENT TetrisGame)

# Player README
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/README_DIST.txt
  DESTINATION .
  RENAME README.txt
  COMPONENT TetrisGame
)

# Microsoft Visual C++ Redistributable (if downloaded or present)
if(EXISTS "${CMAKE_BINARY_DIR}/vc_redist.x64.exe")
  install(FILES "${CMAKE_BINARY_DIR}/vc_redist.x64.exe"
    DESTINATION .
    COMPONENT TetrisGame
  )
endif()

