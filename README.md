# Splatterhouse Recompiled — Xbox 360 Static Recompilation Port

Port nativo para PC de **Splatterhouse (2010)** versión Xbox 360 mediante **recompilación estática** con [XenonRecomp](https://github.com/hedge-dev/XenonRecomp) + [XenonUtils](https://github.com/hedge-dev/XenonUtils).

> Inspirado en proyectos como `Sonic Unleashed Recompiled` y `Ninja Gaiden 2 Black Recompiled`.  
> No contiene código ni assets del juego. Necesitas tu propia copia (disco/ISO) para generar el port.

## Estado

`v0.1.0 — Bootstrap` — Proyecto en blanco compilable. Sin código recompilado aún.

- [x] Scaffolding CMake + SDL3 + spdlog + toml++
- [x] HLE stubs (xboxkrnl, xam, filesystem, threads, memory)
- [x] Backends GPU/Audio/Input (stubs)
- [x] Pipeline `tools/recompile.py`
- [ ] Recompilación real del `default.xex`
- [ ] Implementación HLE completa (D3D9, XAudio2, XInput, STFS)
- [ ] Patches (60 FPS, resolución, etc.)

## Requisitos

| Herramienta | Versión mínima | Notas |
|---|---|---|
| Windows 10/11 o Linux | — | Win recomendado para D3D12 |
| Visual Studio 2022 | 17.8+ | Con `Desktop development with C++` |
| CMake | 3.28+ | `winget install Kitware.CMake` |
| Ninja | 1.11+ | `winget install Ninja-build.Ninja` |
| Clang | 17+ | Opcional, MSVC ok |
| Python | 3.10+ | Para `tools/recompile.py` |
| Git | 2.40+ | Submódulos |

Librerías (se obtienen vía submódulos o FetchContent automáticamente):

- SDL3, spdlog, tomlplusplus, imgui, XenonRecomp, XenonUtils

## Quick Start (Windows)

```powershell
# 1. Clonar (si ya tienes la carpeta, omite)
git clone --recursive <repo> decomp-splatterhouse
cd decomp-splatterhouse

# 2. Setup (instala deps si falta)
powershell -ExecutionPolicy Bypass -File tools/setup.ps1 -InstallTools
# o manual:
git submodule update --init --recursive

# 3. Build bootstrap (sin XEX, solo valida toolchain)
cmake --preset windows-release
cmake --build build/windows-release -j

# 4. Ejecuta (mostrará ventana vacía hasta recompilar)
./build/windows-release/splatterhouse.exe
```

## Recompilación con tu XEX

Necesitas tu disco de Splatterhouse Xbox 360 dumpeado.

```powershell
# Extrae el ISO (exiso, God2Iso, etc.) -> obtienes default.xex
# Desencripta el XEX (requiere xextool o similar):
xextool -r -c game/default.xex
# -> genera game/default.dec.xex

# O deixa que el script lo intente:
python tools/recompile.py --xex game/default.xex --decrypt

# Recompila:
python tools/recompile.py --xex game/default.dec.xex

# Recompila y compila el port:
cmake --preset windows-release
cmake --build build/windows-release -j
```

Config de recompilación: `config/recomp.toml` (copia desde `recomp.toml.example`).

## Estructura

```
.
├── CMakeLists.txt            # Build principal + FetchContent deps
├── CMakePresets.json         # presets windows/linux debug/release
├── vcpkg.json                # deps alternativas via vcpkg
├── config/
│   ├── recomp.toml.example   # config XenonRecomp
│   ├── patches.toml.example  # patches de binario
│   └── config.toml.example   # config runtime (ventana/audio)
├── src/
│   ├── main.cpp              # entry host, loop SDL
│   ├── core/  (config, memory, threads)
│   ├── os/    (hle_kernel, hle_xam, hle_xboxkrnl, filesystem)
│   ├── gpu/   (gpu_backend, d3d12_backend)
│   ├── audio/ (audio_backend SDL3)
│   ├── input/ (input SDL3 -> XInput HLE)
│   └── ppc/   (ppc_context, mmio)
├── include/splatterhouse/    # headers públicos
├── extern/                   # submódulos (XenonRecomp, SDL, etc.)
├── recompiled/               # código C++ generado (no commitear)
├── tools/ (recompile.py, setup.ps1)
└── docs/
```

## Pipeline XenonRecomp

1. **Dump**: Extrae `default.xex` + carpeta `Data/` del disco a `game/`.
2. **Decrypt**: `game/default.dec.xex`.
3. **Analyze**: XenonRecomp analiza PPC y genera `recompiled/*.cpp/.h`.
4. **HLE**: Los imports a `xboxkrnl/xam/d3d9/xaudio` se redirigen a `src/os/*`.
5. **Build**: CMake compila `recompiled/` + `src/` -> `splatterhouse.exe`.
6. **Run**: El exe carga `game/` como filesystem host.

Ver `docs/recompilation.md` para detalles.

## Config runtime

`config.toml` junto al exe (se crea solo la primera vez):

```toml
[window]
width = 1280
height = 720
vsync = true
maxFps = 0

[game]
root = "game"
```

## Roadmap

- [ ] Integrar XenonUtils PPCContext real (fibers, MMU)
- [ ] D3D9 HLE -> D3D12 (Xenos shader translator)
- [ ] XAudio2/XMA -> SDL Audio
- [ ] XInput -> SDL Gamepad
- [ ] STFS/XContent save
- [ ] Bink video -> ffmpeg
- [ ] Patches: unlock FPS, 4K, FOV, ultrawide, skip intros

## Legal

Este proyecto **no distribuye** código, binarios ni assets de Splatterhouse. Es una herramienta de interoperabilidad que requiere que el usuario posea el juego original. Splatterhouse es marca de Bandai Namco. XenonRecomp es de hedge-dev.

## Contribuir

PRs bienvenidos. Usa `clang-format` y `CMakePresets`.

## Créditos

- [hedge-dev/XenonRecomp](https://github.com/hedge-dev/XenonRecomp) y XenonUtils
- Xenia Project (referencia HLE)
- SDL3, spdlog, tomlplusplus
