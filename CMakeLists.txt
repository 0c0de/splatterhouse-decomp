# splatterhouse - ReXGlue Recompiled Project
#
# SDK boilerplate lives in generated/rexglue.cmake.

cmake_minimum_required(VERSION 3.25)
project(splatterhouse LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Discover the SDK from the vendored submodule; install or REXSDK_DIR overrides.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/extern/rexglue-sdk/CMakeLists.txt"
    AND NOT DEFINED REXSDK_DIR)
  set(REXSDK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/extern/rexglue-sdk" CACHE PATH
      "Path to ReXGlue SDK source tree")
endif()

include(generated/rexglue.cmake)

# Sources
set(SPLATTERHOUSE_SOURCES
    src/main.cpp
    src/sh_60fps_hook.cpp
    src/sh_language_hook.cpp
)

if(WIN32)
    add_executable(splatterhouse WIN32 ${SPLATTERHOUSE_SOURCES})
else()
    add_executable(splatterhouse ${SPLATTERHOUSE_SOURCES})
endif()

# GPU_PLUGINS xenos: compila y copia el plugin grafico Xenos (D3D12/Vulkan)
rexglue_setup_target(splatterhouse GPU_PLUGINS xenos)

# El host compila rex_app.cpp del SDK, que incluye imgui.h directamente.
target_include_directories(splatterhouse PRIVATE extern/rexglue-sdk/thirdparty/imgui)
