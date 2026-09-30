# Splatterhouse Recompiled — Xbox 360 Static Recompilation Port

Port nativo para PC de **Splatterhouse (2010)** versión Xbox 360 mediante **recompilación estática** con la [ReXGlue SDK](https://github.com/rexglue/rexglue-sdk).

> Inspirado en proyectos como `Sonic Unleashed Recompiled` y `Ninja Gaiden 2 Black Recompiled`.
> No contiene código ni assets del juego. Necesitas tu propia copia (disco/ISO) para generar el port.

## Estado

`v0.2.0 — ReXGlue Migration` — Migrada la toolchain desde XenonRecomp a ReXGlue.

- [x] Scaffolding ReXGlue (`rexglue init`: CMake, ReXApp, manifest)
- [x] Runtime ReXGlue (memoria guest 4 GB, Xbox360 kernel shims, VFS/STFS, threading)
- [x] Backends GPU (D3D12/Vulkan), Audio (XMA/SDL), Input (XInput/SDL) del SDK
- [x] Codegen vía `rexglue codegen` + manifest (`splatterhouse_manifest.toml`)
- [x] Config codegen portado (`config/codegen.toml`, flags idénticos a los de XenonRecomp)
- [x] ReXApp bootstrap compilable (sin XEX)
- [ ] Recompilación real del `default.dec.xex` (requiere Copia propia)
- [ ] Overrides/patches de juego (60 FPS, resolución, etc.)

## Qué cambió respecto a la version XenonRecomp (v0.1.0)

| Antes (XenonRecomp)                    | Ahora (ReXGlue)                                       |
|----------------------------------------|-------------------------------------------------------|
| `XenonRecomp config/recomp.toml`       | `rexglue codegen splatterhouse_manifest.toml`         |
| `XenonUtils` (ppc_context.h, XEX)      | Runtime del SDK (`rex::`), claves config equivalentes |
| Runtime propio (memoria, threads, HLE) | SDK: `KernelState`, `Memory`, `FunctionDispatcher`    |
| `src/main.cpp` custom (SDL + glue)     | `ReXApp` (windowing + ImGui + runtime lifecycle)      |
| HLE stubs propias (`src/os/*`)         | Shims de kernel incluidos (xboxkrnl/xam genéricos)    |
| GCC/Clang/MSVC cualquiera              | ReXGlue requiere **Clang 18+**                        |

El código del runtime propio quedó en `legacy/` (solo referencia; no compila con el nuevo stack).

## Requisitos

| Herramienta | Versión mínima | Notas |
|---|---|---|
| Windows 10/11 o Linux | — | Win recomendado para D3D12 |
| Clang | 18+ (20.x recomendado) | `winget install LLVM.LLVM` o VS2022 Clang component |
| CMake | 3.25+ | `winget install Kitware.CMake` |
| Ninja | 1.11+ | `winget install Ninja-build.Ninja` |
| Python | 3.10+ | Para `tools/recompile.py` |
| Git | 2.40+ | Submódulos |

Todas las librerias (SDL3, ImGui, fmt, spdlog, GoogleTest…) traen como submódulos internos `extern/rexglue-sdk` (traídos cuando usan `--recursive`).

## Quick Start (Windows)

```powershell
# 1. Setup (instala deps si falta)
powershell -ExecutionPolicy Bypass -File tools/setup.ps1 -InstallTools
# o manual:
git submodule update --init extern/rexglue-sdk
git -C extern/rexglue-sdk submodule update --init --recursive

# 2. Build bootstrap (sin XEX, valida toolchain)
cmake --preset win-amd64-release
cmake --build out/build/win-amd64-release -j

# 3. Obtener el CLI rexglue (dos opciones)
#    a) Descargar release precompilado: https://github.com/rexglue/rexglue-sdk/releases
#    b) Compilar e instalar el SDK:
#       cd extern/rexglue-sdk
#       cmake --preset win-amd64 && cmake --build out/build/win-amd64 --target install

# 4. Coloca tu copia del juego (extraida del ISO):
#    game/default.dec.xex        (desencriptado: xextool -r -c default.xex)
#    game/...                    (resto de datos extraídos)

# 5. Codegen
python tools/recompile.py --xex game/default.dec.xex
#    ó directo: rexglue codegen splatterhouse_manifest.toml

# 6. Build con el código guest generado y compilar
cmake --build out/build/win-amd64-release -j
```

`rexglue codegen` reporta llamadas sin resolver (`UnresolvedCall`): es normal la primera vez. Usa `--force` para generar y refina `config/codegen.toml` (`[functions]`, `[[switch_tables]]`, `[[invalid_instructions]]`, `[[midasm_hook]]`, `[rexcrt]`) iterativamente.

## Estructura del proyecto

```
generated/                  # código recompilado (autogenerado por rexglue codegen)
  rexglue.cmake             #   integración SDK (regenerada por `rexglue migrate`)
config/
  codegen.toml              # flags de codegen + overrides (functions/hooks/rexcrt)
src/
  main.cpp                  # REX_DEFINE_APP + fallback bootstrap sin XEX
  splatterhouse_app.h       # rex::ReXApp subclass (hooks OnPreSetup, OnPostSetup...)
splatterhouse_manifest.toml # manifest: XEX del entrypoint, salida, includes
extern/rexglue-sdk/         # SDK (submódulo)
game/                       # TU copia del juego (gitignored)
legacy/                     # runtime XenonRecomp antiguo (referencia)
```

## Disclaimer

Este proyecto es con fines educativos. No está afiliado a Microsoft ni a Konami. ReXGlue no está afiliado a Microsoft ni a Xbox. No fomenta la piratería: necesitas tu propia copia del juego.
