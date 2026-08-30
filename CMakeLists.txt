cmake_minimum_required(VERSION 3.28)
project(splatterhouse_recompiled
    VERSION 0.1.0
    DESCRIPTION "Splatterhouse (2010) Xbox 360 Static Recompilation Port - XenonRecomp"
    LANGUAGES C CXX
)

# ── Requisitos ──────────────────────────────────────────────────────────
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_C_STANDARD 11)

# Opciones del proyecto
option(SPLATTERHOUSE_ENABLE_LTO "Enable LTO/IPO" OFF)
option(SPLATTERHOUSE_ENABLE_AVX2 "Enable AVX2 (if available)" OFF)
option(SPLATTERHOUSE_FETCH_DEPS "Fetch dependencies via FetchContent if submodules missing" ON)
option(SPLATTERHOUSE_BUILD_TOOLS "Build recompiler tools" ON)

if(SPLATTERHOUSE_ENABLE_LTO)
    include(CheckIPOSupported)
    check_ipo_supported(RESULT ipo_supported OUTPUT ipo_error)
    if(ipo_supported)
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)
    endif()
endif()

# ── Dependencias ────────────────────────────────────────────────────────
include(FetchContent)

# SDL3 - ventana, input, 3D context
if(EXISTS "${CMAKE_SOURCE_DIR}/extern/SDL/CMakeLists.txt")
    add_subdirectory(extern/SDL)
else()
    if(SPLATTERHOUSE_FETCH_DEPS)
        message(STATUS "[deps] Fetching SDL3...")
        FetchContent_Declare(SDL3
            GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
            GIT_TAG release-3.2.20
            GIT_SHALLOW TRUE
        )
        set(SDL_SHARED OFF CACHE BOOL "" FORCE)
        set(SDL_STATIC ON CACHE BOOL "" FORCE)
        set(SDL_TEST OFF CACHE BOOL "" FORCE)
        FetchContent_MakeAvailable(SDL3)
    else()
        message(FATAL_ERROR "SDL3 not found in extern/SDL. Run git submodule update --init")
    endif()
endif()

# spdlog - v1.15+ requerido para Clang 19/20 (fix consteval FMT_STRING)
if(EXISTS "${CMAKE_SOURCE_DIR}/extern/spdlog/CMakeLists.txt")
    add_subdirectory(extern/spdlog)
else()
    if(SPLATTERHOUSE_FETCH_DEPS)
        FetchContent_Declare(spdlog
            GIT_REPOSITORY https://github.com/gabime/spdlog.git
            GIT_TAG v1.15.1
            GIT_SHALLOW TRUE
        )
        FetchContent_MakeAvailable(spdlog)
    endif()
endif()

# tomlplusplus (header-only)
if(EXISTS "${CMAKE_SOURCE_DIR}/extern/tomlplusplus/toml.hpp")
    add_library(tomlplusplus INTERFACE)
    target_include_directories(tomlplusplus INTERFACE extern/tomlplusplus/include)
else()
    if(SPLATTERHOUSE_FETCH_DEPS)
        FetchContent_Declare(tomlplusplus
            GIT_REPOSITORY https://github.com/marzer/tomlplusplus.git
            GIT_TAG v3.4.0
            GIT_SHALLOW TRUE
        )
        FetchContent_MakeAvailable(tomlplusplus)
    endif()
endif()

# XenonRecomp - tool (host only, no need to link if not building tools)
# Debe ir ANTES que XenonUtils porque XenonUtils ahora vive dentro de XenonRecomp/XenonUtils
if(SPLATTERHOUSE_BUILD_TOOLS AND EXISTS "${CMAKE_SOURCE_DIR}/extern/XenonRecomp/CMakeLists.txt")
    add_subdirectory(extern/XenonRecomp)
endif()

# XenonUtils - runtime helpers for recompiled PPC code
# Puede estar en extern/XenonUtils (viejo) o en extern/XenonRecomp/XenonUtils (nuevo)
if(EXISTS "${CMAKE_SOURCE_DIR}/extern/XenonUtils/CMakeLists.txt")
    add_subdirectory(extern/XenonUtils)
elseif(TARGET XenonUtils)
    message(STATUS "[deps] XenonUtils provisto por extern/XenonRecomp")
else()
    message(WARNING "XenonUtils not found. Recompiled code will not build until submodules are fetched.\n  Run: git -C extern/XenonRecomp submodule update --init --recursive")
endif()

# ── Código recompilado (generado por XenonRecomp) ─────────────────────
# Estos ficheros se generan a partir del .xex con:
#   XenonRecomp config/recomp.toml
# Se compilan solo si existen.
file(GLOB_RECURSE RECOMPILED_SOURCES
    "${CMAKE_SOURCE_DIR}/recompiled/*.cpp"
    "${CMAKE_SOURCE_DIR}/recompiled/*.c"
)
file(GLOB_RECURSE RECOMPILED_HEADERS
    "${CMAKE_SOURCE_DIR}/recompiled/*.h"
    "${CMAKE_SOURCE_DIR}/recompiled/*.hpp"
)

# ── Ejecutable principal ──────────────────────────────────────────────
set(SRC
    src/main.cpp
    src/core/config.cpp
    src/core/memory.cpp
    src/core/threads.cpp
    src/os/hle_kernel.cpp
    src/os/hle_xam.cpp
    src/os/hle_xboxkrnl.cpp
    src/gpu/gpu_backend.cpp
    src/gpu/d3d12_backend.cpp
    src/audio/audio_backend.cpp
    src/input/input.cpp
    src/ppc/ppc_context.cpp
    src/ppc/ppc_mmio.cpp
    # Stubs HLE adicionales
    src/os/hle_filesystem.cpp
)

if(RECOMPILED_SOURCES)
    list(APPEND SRC ${RECOMPILED_SOURCES})
    list(APPEND SRC src/os/hle_imports_generated.cpp)
    message(STATUS "[recompiled] Found ${RECOMPILED_SOURCES}")
else()
    message(STATUS "[recompiled] No recompiled sources yet (recompiled/*.cpp). Building bootstrap only.")
endif()

add_executable(splatterhouse ${SRC} ${RECOMPILED_HEADERS})

target_include_directories(splatterhouse PRIVATE
    include
    recompiled
    src
    ${CMAKE_SOURCE_DIR}/extern/XenonUtils/include
    ${CMAKE_SOURCE_DIR}/extern/XenonRecomp/XenonUtils
)

target_link_libraries(splatterhouse PRIVATE
    SDL3::SDL3
    spdlog::spdlog
    tomlplusplus::tomlplusplus
)

# Link XenonUtils if available
if(TARGET XenonUtils)
    target_link_libraries(splatterhouse PRIVATE XenonUtils)
endif()

# Platform specifics
if(WIN32)
    target_link_libraries(splatterhouse PRIVATE winmm ws2_32)
    target_compile_definitions(splatterhouse PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
    # No WIN32_EXECUTABLE para que main() funcione con Clang/MSVC (mantiene consola)
elseif(UNIX)
    target_link_libraries(splatterhouse PRIVATE pthread dl)
endif()

# D3D12 opcional en Windows
if(WIN32)
    target_link_libraries(splatterhouse PRIVATE d3d12 dxgi dxguid)
endif()

# Map file para diagnostico de crashes (direcciones -> funciones)
if(MSVC)
    target_link_options(splatterhouse PRIVATE "/MAP:${CMAKE_CURRENT_BINARY_DIR}/splatterhouse.map")
endif()

target_compile_features(splatterhouse PRIVATE cxx_std_23)

# Warnings
if(MSVC)
    target_compile_options(splatterhouse PRIVATE /W4 /permissive- /Zc:preprocessor)
    if(SPLATTERHOUSE_ENABLE_AVX2)
        target_compile_options(splatterhouse PRIVATE /arch:AVX2)
    endif()
else()
    target_compile_options(splatterhouse PRIVATE -Wall -Wextra -Wpedantic)
    if(SPLATTERHOUSE_ENABLE_AVX2)
        target_compile_options(splatterhouse PRIVATE -mavx2 -mfma)
    endif()
endif()

# ── Instalación ───────────────────────────────────────────────────────
install(TARGETS splatterhouse RUNTIME DESTINATION .)
install(DIRECTORY config/ DESTINATION config OPTIONAL)
install(FILES README.md DESTINATION . OPTIONAL)

# ── Tests (placeholder) ───────────────────────────────────────────────
enable_testing()
