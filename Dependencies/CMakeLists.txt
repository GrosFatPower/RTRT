add_subdirectory(glfw)
add_subdirectory(glm)

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
    add_library(imgui ${imgui_sources})
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
