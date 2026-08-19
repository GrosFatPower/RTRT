add_subdirectory(glfw)
add_subdirectory(glm)

add_library(nlohmann_json INTERFACE)
add_library(nlohmann_json::nlohmann_json ALIAS nlohmann_json)
target_include_directories(nlohmann_json SYSTEM
  INTERFACE nlohmann-json/single_include
)

if (RTRT_BUILD_IMGUI)
  set(imgui_dir imgui)
  set(imgui_sources
    ${imgui_dir}/imconfig.h
    ${imgui_dir}/imgui.cpp
    ${imgui_dir}/imgui.h
    ${imgui_dir}/imgui_demo.cpp
    ${imgui_dir}/imgui_draw.cpp
    ${imgui_dir}/imgui_internal.h
    ${imgui_dir}/imgui_tables.cpp
    ${imgui_dir}/imgui_widgets.cpp
	${imgui_dir}/backends/imgui_impl_glfw.h
	${imgui_dir}/backends/imgui_impl_glfw.cpp
	${imgui_dir}/backends/imgui_impl_opengl3.h
	${imgui_dir}/backends/imgui_impl_opengl3.cpp
  )
  
  if (RTRT_IMGUI_IMGUI_SHARED)
    add_library(imgui SHARED ${imgui_sources})
    install(TARGETS imgui DESTINATION ./lib/)
  else()
    add_library(imgui STATIC ${imgui_sources})
  endif()
  
  target_include_directories(imgui
    PUBLIC ${imgui_dir}
	PUBLIC ${imgui_dir}/backends
	PUBLIC glfw/include
  )
  
  target_link_libraries(imgui
    PUBLIC glfw
  )
endif()

set(imguizmo_dir imguizmo)
add_library(imguizmo STATIC
  ${imguizmo_dir}/ImGuizmo.cpp
  ${imguizmo_dir}/ImGuizmo.h
)

target_include_directories(imguizmo
  PUBLIC ${imguizmo_dir}
)

target_compile_definitions(imguizmo
  PRIVATE IMGUI_DEFINE_MATH_OPERATORS
)

if (MSVC)
  target_compile_options(imguizmo
    PRIVATE /FI"${CMAKE_CURRENT_SOURCE_DIR}/imguizmo_compat/ImGuizmoImGuiCompat.h"
  )
else()
  target_compile_options(imguizmo
    PRIVATE -include "${CMAKE_CURRENT_SOURCE_DIR}/imguizmo_compat/ImGuizmoImGuiCompat.h"
  )
endif()

target_link_libraries(imguizmo
  PUBLIC imgui
)
