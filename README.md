# Splatterhouse Recompiled — Port nativo de Xbox 360 a PC

Port nativo para PC de **Splatterhouse (2010)** (versión Xbox 360, title id `4E4D07F0`) mediante **recompilación estática** con [ReXGlue SDK](https://github.com/rexglue/rexglue-sdk). El código del juego se traduce a C++ nativo que se compila para tu PC: **no es un emulador**, no hay interpretación en runtime y corre a velocidad nativa (60 FPS).

> Inspirado en `Sonic Unleashed Recompiled`, `Ninja Gaiden 2 Black Recompiled` y `Silent Hill: Downpour Recompiled`.
> **No contiene código ni assets del juego.** Necesitas tu propia copia (disco/ISO).

---

## Estado

**Jugable.** Arranca, se navega por los menús y se juega la campaña a **60 FPS** con iluminación correcta, texto localizado y audio en inglés.

### Funciona
- **Recompilación estática completas**: 208 ficheros, sin crashes conocidos en gameplay.
- **GPU (D3D12, ruta ROV)**: escena 3D correcta (ROV reproduce la eDRAM del X360; RTV rompe la iluminación).
- **60 FPS**: parche oficial de 60 FPS aplicado de forma durable (ver abajo).
- **FSR (FidelityFX)**: reescalado/AA en el present.
- **Localización (texto)**: menú del juego traducible a español/francés/italiano/alemán/japonés; audio siempre inglés.
- **Instalador**: arrastras la ISO y listo.
- **Caché de shaders precompilada** para evitar tirones.

### Pendiente / limitaciones conocidas
- **Tirones al compilar shaders** en la primera partida (el juego compila sus materiales y el port los traduce a D3D12). Mitigado con la caché de shaders (`shadercache/`).
- **Iluminación con RTV**: si pones la ruta `rtv` la escena casi se apaga (limitación de diseño de la ruta RTV; **hay que usar `rov`**). Ver `docs/rov-performance.md`.
- **Rendimiento de ROV**: la vía de fondo para GPUs modestas es optimizar el *pixel shader* de ROV (el SDK lo deja como TODO: especializar con parámetros de RT estáticos). De momento se compensa con **presets** de resolución + FSR.
- **Menú de opciones in-game**: pantalla propia con estilo de juego (tecla **F5**), no integrada literalmente en el menú Scaleform del juego (no se puede editar el `.gfx`).
- **Post-procesado/DoF** un poco excesivo al inicio (probable upscale 720p→ventana).
- **Distribución**: solo binarios Win x64 (D3D12).

---

## Cómo se hizo esta decompilación

No hay código fuente: se parte del **XEX retail** y se **recompila estáticamente**.

1. **Extracción de la ISO** (`tools/extract-xiso.exe`): se obtiene `default.xex` + `data/`.
2. **Desencriptado del XEX** (`tools/xextool.exe -e u -c b`): genera `default.dec.xex` (solo para el codegen).
3. **Codegen** (`rexglue codegen splatterhouse_manifest.toml`): ReXGlue analiza el PPC del XEX, descubre funciones y emite **C++ recompilado** (`generated/*.cpp`, ~208 ficheros) + registro de funciones e imagen.
4. **Runtime** (SDK): memoria guest 4 GB, shims de kernel xboxkrnl/xam, VFS/STFS, threading, GPU (D3D12), audio (XMA/SDL), input.
5. **Parches locales del SDK** (ver `AGENTS.md`) para corregir bugs de codegen (propagación de registros no-ABI `r0/r2/r11/r12` → arregla `__finally`/`__chkstk`), descubrir funciones por punteros de datos, implementar `vmaxuw`, instrumentar access violations, etc.
6. **Parches del juego** aplicados de forma durable en `src/` (no en `generated/`):
   - **60 FPS**: datos (`0x82F8FF58/5C`: 1/15 y 1/30 → **1/60**) y una instrucción (`0x8247A4AC`) replicada con un **override fuerte** de la función weak del codegen.
   - **Localización**: override de `sub_827B65E0` para forzar el idioma **solo** en la ruta de carga del idioma (texto), dejando el audio en inglés.

El runtime propio antiguo (XenonRecomp) quedó en `legacy/` (solo referencia).

---

## Instalación para el usuario final (PC)

**Requisitos**: Windows 10/11 x64, GPU con **D3D12 + Rasterizer-Ordered Views** (NVIDIA/AMD modernos), y tu copia de Splatterhouse (ISO de Xbox 360).

1. Descarga y descomprime el release (el `.exe`, las DLLs, `install.bat` y `tools/`).
2. **Arrastra tu ISO de Splatterhouse sobre `install.bat`**.
   - El script extrae la ISO (`extract-xiso`), la mueve a `assets\`, desencripta el XEX con `xextool` (`assets\default.dec.xex`) y crea un `splatterhouse.toml` por defecto.
   - Si incluyes una carpeta `shadercache\` en el release, la copia a tu caché de shaders (menos tirones).
3. Ejecuta **`splatterhouse.exe`** y a jugar.

Sin `splatterhouse.toml` no se carga la GPU (pantalla negra); el instalador lo crea por ti y `SplatterhouseApp::OnPreSetup` fuerza `gpu_plugin="xenos"` como red de seguridad.

---

## Compilar desde el código (desarrolladores)

### Requisitos
| Herramienta | Versión | Notas |
|---|---|---|
| Windows 10/11 | — | D3D12 |
| VS 2022/2026 (Clang) o LLVM | Clang 18+ (probado 22.1.8) | |
| CMake | 3.25+ | |
| Ninja | 1.11+ | |
| Python | 3.9+ | scripts de `tools/` |
| Git | 2.40+ | submódulos |

### Pasos

```powershell
# 0) Submódulo del SDK
git submodule update --init extern/rexglue-sdk
git -C extern/rexglue-sdk submodule update --init --recursive

# 1) Configurar (relwithdebinfo = rinde bien; debug = lento)
cmake --preset win-amd64-relwithdebinfo
#    (opcional FSR)  -DREXGLUE_ENABLE_FIDELITYFX=ON

# 2) Colocar tu copia del juego (gitignored)
#    game/default.xex   (retail, tal cual sale de la ISO)
#    game/default.dec.xex (desencriptado con xextool -e u -c b; para el codegen)
#    game/data/...      (resto de la ISO)

# 3) Build (el codegen se ejecuta solo si el stamp está caducado)
cmake --build out\build\win-amd64-relwithdebinfo --target splatterhouse -j 6
#    regenerar codegen a mano (~800 s, 208 ficheros):
#    extern\rexglue-sdk\out\win-amd64\rexglued.exe codegen splatterhouse_manifest.toml --ignore-stamp
```

El exe carga las DLLs desde **su propio directorio**: tras reconstruir `rexruntime` hay que copiar `extern\rexglue-sdk\out\win-amd64\rexruntimerd.dll` (y las demás `*rd.dll`) a `out\build\win-amd64-relwithdebinfo\`.

Con **FSR** (`-DREXGLUE_ENABLE_FIDELITYFX=ON`) se genera además **`amd_fidelityfx_dx12drel.dll`** (en `bin/`), que debe ir junto al exe **y** junto a `rexruntimerd.dll`/`rexglued.exe` o el codegen falla con `0xC0000135`.

---

## Controles y opciones

| Tecla | Acción |
|---|---|
| **F3** | Overlay de debug (FPS, stats) |
| **F4** | Overlay de ajustes del runtime (todas las cvars) |
| **F5** | **Pantalla de OPCIONES del port** (idioma, preset gráfico, FSR, VSync, límite de FPS…) |
| **F7** | Logros |
| **`** (backtick) | Consola |

En la pantalla **F5**: flechas para navegar/cambiar, **S** guarda en `splatterhouse.toml`, Esc/F5 cierra. Las filas con `[reinicio]` requieren reiniciar.

Presets: **Bajo** 960x540 / **Medio** 1280x720 / **Alto** 1600x900 / **Ultra** 1920x1080 (ajustan resolución interna, FSR y caché de texturas).

---

## Configuración (`splatterhouse.toml`, junto al exe)

| cvar | valores | notas |
|---|---|---|
| `gpu_plugin` | `xenos` | **obligatorio** (vacío = sin GPU = pantalla negra) |
| `render_target_path_d3d12` | `rov` | **obligatorio** (`rtv` rompe la iluminación) |
| `present_effect` | `fsr` (o `bilinear`, `cas`) | reescalado del present |
| `video_mode_width/height` | p.ej. `1920/1080` | resolución interna del guest (bájala para ganar FPS) |
| `video_mode_refresh_rate` | `60` | |
| `vsync` | `true` | |
| `d3d12_present_frame_limiter[_fps]` | `false` / `30`…`144` | límite de FPS |
| `sh_language` | `auto`/`spanish`/`english`/… | idioma del **texto** (audio siempre inglés) |
| `sh_60fps` | `true` | parche de 60 FPS |
| `log_level` | `warn` / `trace` | |

---

## Estructura del proyecto

```
install.bat                 # instalador (arrastra la ISO)
assets/                     # datos del juego instalados (gitignored; junction a game/ en dev)
tools/
  extract-xiso.exe          # extraer la ISO de Xbox 360
  xextool.exe               # desencriptar el XEX
  setup.ps1 / recompile.py  # setup y codegen (utilidades de desarrollo)
generated/                  # código recompilado (autogenerado; gitignored)
config/codegen.toml         # flags de codegen + overrides [functions]
src/
  main.cpp                  # REX_DEFINE_APP + cvars (sh_60fps, sh_language, sh_graphics_menu)
  splatterhouse_app.h       # ReXApp: parches, rutas, menú, XEX
  sh_60fps_hook.cpp         # parche 60 FPS (instrucción)
  sh_language_hook.cpp      # localización del texto
  graphics_menu.h           # pantalla OPCIONES (F5)
extern/rexglue-sdk/         # SDK (submódulo, con parches locales)
docs/rov-performance.md     # análisis de rendimiento de ROV
AGENTS.md                   # bitácora técnica completa (estado, parches, decisiones)
legacy/                     # runtime XenonRecomp antiguo (referencia)
```

---

## Créditos y aviso legal

- **ReXGlue SDK** — runtime y toolchain de recompilación.
- **Xenia** — la GPU/eDRAM, formatos y muchas bases del runtime provienen de Xenia.
- **extract-xiso** (xboxdev) y **XexTool** (xorloser) — herramientas de la ISO/XEX.
- **Parche de 60 FPS**: de `xenia-canary/game-patches` (autor *illusion*), adaptado al recomp.
- **FidelityFX / FSR**: AMD.

Este proyecto es con fines **educativos y de preservación**. No está afiliado a Microsoft, Bandai Namco ni Konami, y **no incluye código ni assets del juego**: necesitas tu propia copia legal para usarlo.
