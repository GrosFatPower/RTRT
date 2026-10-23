cmake_minimum_required(VERSION 3.20)

project(RTRT
  LANGUAGES C CXX
  VERSION 1.0
)

include(CTest)
option(RTRT_BUILD_RENDER_REGRESSION_TESTS "Build local render regression tests" ON)

SET_PROPERTY(GLOBAL PROPERTY USE_FOLDERS ON)

# Detection of the system architecture
message(STATUS "CMAKE_SYSTEM_PROCESSOR=${CMAKE_SYSTEM_PROCESSOR}")
if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64|aarch64|ARM64")
  set(ARCH_ARM64 TRUE)
  message(STATUS "Detected ARM64 architecture, using arm64-osx triplet")
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
  set(ARCH_X64 TRUE)
  message(STATUS "Detected x64 architecture, using x64-osx triplet")
endif()

# C++ configuration
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Configuration for compilers
if(MSVC)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W3")
  set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /O2 /Oi /GL /MD")
  set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /Od /Zi")

  # Support AVX2 on x64 Windows
  if(ARCH_X64)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /arch:AVX2")
  endif()
    
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wno-unused-parameter -Wpedantic -Wshadow -Wconversion -fcolor-diagnostics")
  set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} --O3 -flto -march=native -mtune=native -fvectorize -fstack-protector-strong -DNDEBUG")
  set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0 -g -g3 -fsanitize=address -fsanitize=undefined")
    
  # SIMD Support based on architecture
  if(ARCH_ARM64)
    # Support ARM NEON
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=armv8-a")
    add_compile_definitions(SIMD_ARM_NEON)
    message(STATUS "Enabling ARM NEON support")
    
    # Check if the compiler supports NEON intrinsics
    include(CheckCXXSourceCompiles)
    check_cxx_source_compiles("
        #include <arm_neon.h>
        int main() { 
            float32x4_t v = vdupq_n_f32(1.0f); 
            return 0; 
        }" HAVE_NEON)
    
    if(NOT HAVE_NEON)
        message(WARNING "ARM NEON intrinsics not supported by compiler")
    endif()
      
  elseif(ARCH_X64)
      # Support AVX2 on x64
      set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mavx2 -mfma")
      add_compile_definitions(SIMD_AVX2)
      message(STATUS "Enabling AVX2 support")	

  endif()
endif()

set(CMAKE_CONFIGURATION_TYPES "Debug;Release;RelWithDebInfo" CACHE STRING "Configuration types" FORCE)

# Native File Dialog
set(NFD_SOURCE_DIR ${CMAKE_SOURCE_DIR}/Dependencies/nativefiledialog/src)
if(WIN32)
  set(NFD_PLATFORM_SOURCE ${NFD_SOURCE_DIR}/nfd_win.cpp)
elseif(APPLE)
  enable_language(OBJC)
  set(NFD_PLATFORM_SOURCE ${NFD_SOURCE_DIR}/nfd_cocoa.m)
else()
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(GTK3 REQUIRED gtk+-3.0)
  set(NFD_PLATFORM_SOURCE ${NFD_SOURCE_DIR}/nfd_gtk.c)
endif()
add_library(nativefiledialog STATIC ${NFD_SOURCE_DIR}/nfd_common.c ${NFD_PLATFORM_SOURCE})
target_include_directories(nativefiledialog PUBLIC ${NFD_SOURCE_DIR}/include PRIVATE ${NFD_SOURCE_DIR})
if(WIN32)
  target_link_libraries(nativefiledialog PUBLIC comctl32)
elseif(APPLE)
  find_library(APPKIT_LIBRARY AppKit REQUIRED)
  target_link_libraries(nativefiledialog PUBLIC ${APPKIT_LIBRARY})
  target_compile_options(nativefiledialog PRIVATE -Wno-deprecated-declarations)
else()
  target_include_directories(nativefiledialog PRIVATE ${GTK3_INCLUDE_DIRS})
  target_link_libraries(nativefiledialog PUBLIC ${GTK3_LIBRARIES})
  target_compile_options(nativefiledialog PRIVATE ${GTK3_CFLAGS_OTHER})
endif()

# OpenGL
find_package(OpenGL REQUIRED)

# GLEW
if(APPLE)
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(GLEW REQUIRED glew)

  find_package(GLEW QUIET)
  if(NOT GLEW_FOUND)
    message(WARNING "GLEW NOT found, can't build RTRT")
  endif()
  if(NOT GLEW_LIBRARIES)
    if(GLEW_LIBRARY_DEBUG AND GLEW_LIBRARY_RELEASE)
      set(GLEW_LIBRARIES "optimized;${GLEW_LIBRARY_RELEASE};debug;${GLEW_LIBRARY_DEBUG}")
    else()
      set(GLEW_LIBRARIES GLEW::GLEW)
    endif()
  endif()
else()
  set(GLEW_INCLUDE_DIRS "${CMAKE_SOURCE_DIR}/Dependencies/glew-2.1.0/include")
  set(GLEW_LIBRARIES "glew32s.lib")
endif()

# APPLE specific
if(APPLE)
  find_library(COCOA_LIBRARY Cocoa)
  find_library(IOKIT_LIBRARY IOKit)
  find_library(COREVIDEO_LIBRARY CoreVideo)
endif()

# IMGUI options
option(RTRT_IMGUI_IMGUI_SHARED "Use imgui as a shared library" OFF)
option(RTRT_BUILD_IMGUI "Build imgui using sources provided in external/imgui" ON)
set(RTRT_IMGUI_SOURCE_DIR ${CMAKE_SOURCE_DIR}/Dependencies/imgui CACHE STRING "Source dir of imgui")

# GLM SIMD options
if(ARCH_ARM64)
  message("GLM_ENABLE_SIMD_NEON ON")
  set(GLM_ENABLE_SIMD_NEON ON)
elseif(ARCH_X64)
  message("GLM_ENABLE_SIMD_AVX2 ON")
  set(GLM_ENABLE_SIMD_AVX2 ON)
endif()

# Intel Open Image Denoise (OIDN)
option(RTRT_ENABLE_OIDN "Enable Intel Open Image Denoise (OIDN)" ON)
set(RTRT_HAS_OIDN FALSE)
if(RTRT_ENABLE_OIDN)
  include(FetchContent)
  set(OIDN_VERSION "2.5.1")
  set(OIDN_URL "")
  if(WIN32 AND ARCH_X64)
    set(OIDN_URL "https://github.com/RenderKit/oidn/releases/download/v${OIDN_VERSION}/oidn-${OIDN_VERSION}.x64.windows.zip")
  elseif(APPLE AND ARCH_ARM64)
    set(OIDN_URL "https://github.com/RenderKit/oidn/releases/download/v${OIDN_VERSION}/oidn-${OIDN_VERSION}.arm64.macos.tar.gz")
  elseif(APPLE AND ARCH_X64)
    set(OIDN_URL "https://github.com/RenderKit/oidn/releases/download/v${OIDN_VERSION}/oidn-${OIDN_VERSION}.x86_64.macos.tar.gz")
  elseif(UNIX AND NOT APPLE AND ARCH_X64)
    set(OIDN_URL "https://github.com/RenderKit/oidn/releases/download/v${OIDN_VERSION}/oidn-${OIDN_VERSION}.x86_64.linux.tar.gz")
  elseif(UNIX AND NOT APPLE AND ARCH_ARM64)
    set(OIDN_URL "https://github.com/RenderKit/oidn/releases/download/v${OIDN_VERSION}/oidn-${OIDN_VERSION}.aarch64.linux.tar.gz")
  endif()

  if(NOT OIDN_URL STREQUAL "")
    FetchContent_Declare(
      oidn_sdk
      URL ${OIDN_URL}
      DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    FetchContent_MakeAvailable(oidn_sdk)
    if(EXISTS "${oidn_sdk_SOURCE_DIR}/include/OpenImageDenoise/oidn.hpp")
      set(RTRT_HAS_OIDN TRUE)
      set(OIDN_INCLUDE_DIR "${oidn_sdk_SOURCE_DIR}/include")
      if(WIN32)
        set(OIDN_LIBRARY "${oidn_sdk_SOURCE_DIR}/lib/OpenImageDenoise.lib")
        file(GLOB OIDN_RUNTIME_FILES "${oidn_sdk_SOURCE_DIR}/bin/*.dll")
      elseif(APPLE)
        set(OIDN_LIBRARY "${oidn_sdk_SOURCE_DIR}/lib/libOpenImageDenoise.dylib")
        file(GLOB OIDN_RUNTIME_FILES "${oidn_sdk_SOURCE_DIR}/lib/*.dylib")
      else()
        set(OIDN_LIBRARY "${oidn_sdk_SOURCE_DIR}/lib/libOpenImageDenoise.so")
        file(GLOB OIDN_RUNTIME_FILES "${oidn_sdk_SOURCE_DIR}/lib/*.so*")
      endif()
      message(STATUS "Intel Open Image Denoise (OIDN ${OIDN_VERSION}) enabled: ${oidn_sdk_SOURCE_DIR}")
    endif()
  endif()
endif()

# Dependencies (glfw, glm, nlohmann_json, imgui, imguizmo)
add_subdirectory(Dependencies)

# Shaders
add_subdirectory(Shaders)

# Modular Architecture Subdirectories
add_subdirectory(Engine)
add_subdirectory(Framework)
add_subdirectory(Games/FpsGame)
add_subdirectory(Apps/RenderLab)

if(RTRT_BUILD_RENDER_REGRESSION_TESTS)
  add_subdirectory(Tests)
endif()

# Display build configuration
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "CXX Compiler: ${CMAKE_CXX_COMPILER_ID}")
message(STATUS "CXX Flags: ${CMAKE_CXX_FLAGS}")
if(ARCH_ARM64)
  message(STATUS "SIMD Support: ARM NEON")
  message(STATUS "vcpkg Triplet: arm64-osx")
elseif(ARCH_X64)
  message(STATUS "SIMD Support: AVX2")
  message(STATUS "vcpkg Triplet: x64-osx")
else()
  message(STATUS "SIMD Support: Scalar fallback")
endif()
