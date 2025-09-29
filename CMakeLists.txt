cmake_minimum_required(VERSION 3.20)

project(RTRT
  LANGUAGES CXX
  VERSION 1.0
)

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

add_subdirectory(Source)
add_subdirectory(Dependencies)
add_subdirectory(Shaders)

file(GLOB_RECURSE CORE_SOURCE ./Source/src/*.cpp)
file(GLOB_RECURSE CORE_HEADER ./Source/src/*.h ./Source/src/*.hxx)
file(GLOB_RECURSE EXT_SOURCE ./Dependencies/RadeonRays/*.cpp)
file(GLOB_RECURSE EXT_HEADER ./Dependencies/RadeonRays/*.h)
file(GLOB_RECURSE SHADERS ./Shaders/*.glsl)

source_group("src"    FILES ${CORE_SOURCE} ${EXT_SOURCE})
source_group("header" FILES ${CORE_HEADER} ${EXT_HEADER})
source_group("shaders" FILES ${SHADERS})

SET(EXE_NAME "RT_renderer")
add_executable(${EXE_NAME} ${CORE_SOURCE} ${CORE_HEADER} ${EXT_SOURCE} ${EXT_HEADER} ${SHADERS})

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
											   
# IMGUI
option(RTRT_IMGUI_IMGUI_SHARED "Use imgui as a shared library" OFF)
option(RTRT_BUILD_IMGUI "Build imgui using sources provided in external/imgui" ON)
set(RTRT_IMGUI_SOURCE_DIR ${CMAKE_SOURCE_DIR}/Dependencies/imgui CACHE STRING "Source dir of imgui")

# GLM
if(ARCH_ARM64)
  message("GLM_ENABLE_SIMD_NEON ON")
  set(GLM_ENABLE_SIMD_NEON ON)
elseif(ARCH_X64)
  message("GLM_ENABLE_SIMD_AVX2 ON")
  set(GLM_ENABLE_SIMD_AVX2 ON)
endif()

target_include_directories(${EXE_NAME}
  PUBLIC ${CMAKE_SOURCE_DIR}/Source/src
  PUBLIC ${CMAKE_SOURCE_DIR}/Dependencies/glfw/include
  PUBLIC ${CMAKE_SOURCE_DIR}/Dependencies/imgui
  PUBLIC ${CMAKE_SOURCE_DIR}/Dependencies/stb
  PUBLIC ${CMAKE_SOURCE_DIR}/Dependencies/glm
  PUBLIC ${CMAKE_SOURCE_DIR}/Dependencies/tinyobjloader
  PUBLIC ${CMAKE_SOURCE_DIR}/Dependencies/tinydir
  PUBLIC ${CMAKE_SOURCE_DIR}/Dependencies/tinygltf
  PUBLIC ${CMAKE_SOURCE_DIR}/Dependencies/tinybvh
  PUBLIC ${CMAKE_SOURCE_DIR}/Dependencies/RadeonRays
  PUBLIC ${GLEW_INCLUDE_DIRS}
)

target_link_directories(${EXE_NAME}
  PUBLIC ${CMAKE_SOURCE_DIR}/Dependencies/glew-2.1.0/lib/Release/x64/
)

if(APPLE)
  target_link_libraries(${EXE_NAME}
    glfw
    imgui
    ${GLEW_LIBRARIES}
    ${OPENGL_LIBRARIES}
    ${COCOA_LIBRARY} 
    ${IOKIT_LIBRARY} 
    ${COREVIDEO_LIBRARY}
  )
else()
  target_link_libraries(${EXE_NAME}
    glfw
    imgui
    ${GLEW_LIBRARIES}
    ${OPENGL_LIBRARIES}
  )
endif()

if(WIN32)
  set_target_properties(${EXE_NAME} PROPERTIES
  	VS_DEBUGGER_WORKING_DIRECTORY "$(Project)$(Configuration)"
  )
endif()

target_compile_definitions(${EXE_NAME}
  PUBLIC GLEW_STATIC
  PUBLIC NOMINMAX
)

target_compile_features(${EXE_NAME}
 PRIVATE cxx_std_17
)

if(APPLE)
  target_compile_options(${EXE_NAME} 
    PRIVATE -fexperimental-library -Wno-deprecated-declarations
  )

  if(ARCH_ARM64)
      set_target_properties(${EXE_NAME} PROPERTIES
          OSX_ARCHITECTURES "arm64"
      )
  endif()
endif()

# Define macros for architecture-specific SIMD support
if(ARCH_ARM64)
  target_compile_definitions(${EXE_NAME} PRIVATE SIMD_ARM_NEON)
elseif(ARCH_X64)
  target_compile_definitions(${EXE_NAME} PRIVATE SIMD_AVX2)
endif()

# Set output directories
set_target_properties(${EXE_NAME} PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
  RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/Debug
  RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/Release
)

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
