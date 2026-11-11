set(GLFW_INSTALL OFF CACHE BOOL "Generate installation target" FORCE)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "Build the GLFW documentation" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "Build the GLFW test programs" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "Build the GLFW example programs" FORCE)
add_subdirectory(glfw)
add_subdirectory(glm)

add_library(nlohmann_json INTERFACE)
add_library(nlohmann_json::nlohmann_json ALIAS nlohmann_json)
target_include_directories(nlohmann_json SYSTEM BEFORE
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
  
  target_include_directories(imgui BEFORE
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

target_include_directories(imguizmo BEFORE
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

set(enet_dir enet)
add_library(enet STATIC
  ${enet_dir}/callbacks.c
  ${enet_dir}/compress.c
  ${enet_dir}/host.c
  ${enet_dir}/list.c
  ${enet_dir}/packet.c
  ${enet_dir}/peer.c
  ${enet_dir}/protocol.c
  ${enet_dir}/unix.c
  ${enet_dir}/win32.c
)

target_include_directories(enet
  PUBLIC ${enet_dir}/include
)

if (WIN32)
  target_compile_definitions(enet PRIVATE _WINSOCK_DEPRECATED_NO_WARNINGS)
  target_link_libraries(enet PUBLIC ws2_32 winmm)
elseif (UNIX AND NOT APPLE)
  target_compile_definitions(enet PRIVATE
    HAS_FCNTL=1
    HAS_POLL=1
    HAS_GETADDRINFO=1
    HAS_GETNAMEINFO=1
    HAS_INET_PTON=1
    HAS_INET_NTOP=1
    HAS_MSGHDR_FLAGS=1
    HAS_SOCKLEN_T=1
  )
endif()


# ---------------------------------------------------------------------------
# miniupnpc (UPnP IGD client, Windows / macOS / Linux)
# ---------------------------------------------------------------------------
set(miniupnpc_dir ${CMAKE_CURRENT_SOURCE_DIR}/miniupnpc)
set(PROJECT_VERSION 2.3.3)
configure_file(${miniupnpc_dir}/miniupnpcstrings.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/miniupnpc_gen/miniupnpcstrings.h)

add_library(miniupnpc STATIC
  ${miniupnpc_dir}/src/igd_desc_parse.c
  ${miniupnpc_dir}/src/miniupnpc.c
  ${miniupnpc_dir}/src/minixml.c
  ${miniupnpc_dir}/src/minisoap.c
  ${miniupnpc_dir}/src/minissdpc.c
  ${miniupnpc_dir}/src/miniwget.c
  ${miniupnpc_dir}/src/upnpcommands.c
  ${miniupnpc_dir}/src/upnpdev.c
  ${miniupnpc_dir}/src/upnpreplyparse.c
  ${miniupnpc_dir}/src/upnperrors.c
  ${miniupnpc_dir}/src/connecthostport.c
  ${miniupnpc_dir}/src/portlistingparse.c
  ${miniupnpc_dir}/src/receivedata.c
  ${miniupnpc_dir}/src/addr_is_reserved.c
)

target_include_directories(miniupnpc
  PUBLIC  ${miniupnpc_dir}/include
  PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/miniupnpc_gen
)

target_compile_definitions(miniupnpc PUBLIC MINIUPNP_STATICLIB)

if (WIN32)
  target_compile_definitions(miniupnpc PRIVATE _CRT_SECURE_NO_WARNINGS _WINSOCK_DEPRECATED_NO_WARNINGS _WIN32_WINNT=0x0601)
  target_link_libraries(miniupnpc PUBLIC ws2_32 iphlpapi)
else()
  target_compile_definitions(miniupnpc PRIVATE
    MINIUPNPC_SET_SOCKET_TIMEOUT
    MINIUPNPC_GET_SRC_ADDR
    _BSD_SOURCE
    _DEFAULT_SOURCE
  )
  if (APPLE)
    target_compile_definitions(miniupnpc PRIVATE _DARWIN_C_SOURCE)
  else()
    target_compile_definitions(miniupnpc PRIVATE _XOPEN_SOURCE=600)
  endif()
endif()
