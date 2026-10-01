# AGENTS.md — Splatterhouse Recompiled (X360 → PC)

Port nativo de **Splatterhouse (2010) X360** por **recompilación estática** con [ReXGlue SDK](https://github.com/rexglue/rexglue-sdk). En recompilación estática la "pila invitada" **ES** la pila nativa del host: `ctx->lr` no sirve (devuelve 0); el llamador invitado real aparece como frame `splatterhouse!__imp__sub_*` (resoluble con `llvm-symbolizer --relative-address`).

## Entorno (Windows)

- VS 2026 "VS 18" Community + Ninja + LLVM 22.1.8 (`C:\Program Files\LLVM\bin`). Python 3.9.
- SDK submódulo: `extern/rexglue-sdk` (nightly `0c7b01a`).
- PATH necesario:
  - `C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin`
  - `...\CMake\Ninja` (`ninja.exe`)

## Build

```powershell
cmake --preset win-amd64-debug
cmake --build out\build\win-amd64-debug -j 4            # todo (exe + DLL)
# targets concretos:
#   rexruntime  -> rexruntimed.dll (runtime; salida fija del SDK)
#   rexglue     -> rexglued.exe   (CLI de codegen; re-embebe las plantillas inja)
#   splatterhouse -> el .exe
```

- La DLL **siempre** se emite a `extern\rexglue-sdk\out\win-amd64\rexruntimed.dll` (config de salida fija del CMake del SDK), incluso en Debug.
- El exe carga la DLL desde **su propio directorio** → copiarla tras cada rebuild de `rexruntime`:

```powershell
Copy-Item extern\rexglue-sdk\out\win-amd64\rexruntimed.dll out\build\win-amd64-debug\rexruntimed.dll -Force
Copy-Item extern\rexglue-sdk\out\win-amd64\rexruntimed.pdb out\build\win-amd64-debug\rexruntimed.pdb -Force
```

- El build del exe **auto-ejecuta el codegen** (`rexglued.exe codegen splatterhouse_manifest.toml`) cuando el stamp está caducado. Regeneración completa (208 ficheros): ~800 s. Regenerar a mano:
  ```powershell
  extern\rexglue-sdk\out\win-amd64\rexglued.exe codegen splatterhouse_manifest.toml --ignore-stamp
  ```
- `generated/` está **gitignored**: los ficheros generados se pierden si no se regeneran. Cualquier parche manual en `generated/*.cpp` se pierde al regenerar. **Aviso:** editar a mano un fichero de `generated/` actualiza su mtime y hace que ninja **relance el codegen (~800 s)**; si solo era cosmético, sáltalo con `(Get-Item generated\codegen.build.stamp).LastWriteTime = Get-Date`. Por eso los parches van en `src/` (override weak), no en `generated/`.

> **Rendimiento (importante):** `win-amd64-debug` compila los 208 ficheros recompilados **sin optimizar** → el *command processor* (decodificación PM4) y el código guest se convierten en cuello de botella (pocos FPS + stutter). Para **jugar/medir** usar el preset **`win-amd64-relwithdebinfo`**:
> ```powershell
> cmake --preset win-amd64-relwithdebinfo   # (desde un entorno VS: rc.exe en PATH)
> cmake --build out\build\win-amd64-relwithdebinfo --target splatterhouse -j 6
> ```
> Salida en `out\build\win-amd64-relwithdebinfo\` (DLLs `*rd.dll`); crear ahí la junction `assets`→`game` y copiar `splatterhouse.toml`. El exe Debug y el Release comparten `generated/`.

> **FSR (FidelityFX) — hay que activarlo en el configure:** el presenter D3D12 trae EASU/RCAS/CAS + temporal FSR2/3, pero viene **compilado fuera** (`REXGLUE_ENABLE_FIDELITYFX=OFF`). Para tenerlo:
> ```powershell
> cmake --preset win-amd64-relwithdebinfo -DREXGLUE_ENABLE_FIDELITYFX=ON
> ```
> En Win32 **no** requiere Vulkan SDK; hace FetchContent del fork `rexglue/FidelityFX-SDK` y auto-compila shaders (DXC). Reconstruye `rexruntimerd.dll` y produce **`amd_fidelityfx_dx12drel.dll`** (en `bin/`) que debe copiarse **junto al exe**. Con FFX ON, `present_effect` acepta `bilinear` (defecto), `cas`, `fsr`, `fsr2`, `fsr3`. El preset **Debug** sigue con FFX **OFF** (si lo quieres ahí, pásale el mismo `-D`).
> **Ojo:** con FFX ON, `rexruntimerd.dll` depende de `amd_fidelityfx_dx12drel.dll`; hay que copiarla **también** en `extern\rexglue-sdk\out\win-amd64\` (junto a la runtime y a `rexgluerd.exe`, el codegen), o el **codegen falla con `0xC0000135` (DLL no encontrada)**.

## Instalación para el usuario final

`install.bat` (en la raíz) + **`tools\extract-xiso.exe`**: el usuario **arrastra la ISO** de Splatterhouse sobre `install.bat`. **No se puede montar** una ISO de Xbox 360 (es un disco de video, no UDF/ISO9660 estandar), por eso se extrae con **`extract-xiso -x -s`** (`tools\extract-xiso.exe`, [xboxdev/extract-xiso](https://github.com/xboxdev/extract-xiso)); `-s` salta `$SystemUpdate`. Luego mueve el contenido a **`assets\`** junto al exe (lo que espera `SplatterhouseApp::OnConfigurePaths`) y **desencripta** el XEX con **`tools\xextool.exe -e u -c b -o assets\default.dec.xex assets\default.xex`** (XexTool v6.3 de xorloser). `SplatterhouseApp::OnLoadXexImage` usa `game:\default.dec.xex` si existe en `assets\` (si no, el `default.xex` retail). **Sin `splatterhouse.toml` no se carga la GPU** (`gpu_plugin` vacío → pantalla negra); el instalador **crea un `splatterhouse.toml` por defecto** (`gpu_plugin="xenos"`, `render_target_path_d3d12="rov"`, `present_effect="fsr"`, 1080p, `vsync=false`, `d3d12_present_frame_limiter[_fps]=60`) si no existe, y `SplatterhouseApp::OnPreSetup` también fuerza `gpu_plugin="xenos"`+ROV+FSR como red de seguridad. Solo hay que ejecutar `splatterhouse.exe`.
- **Caché de shaders precompilada** (evita los tirones de la primera partida): el SDK guarda las descripciones de pipeline y los shaders traducidos en `%MyDocuments%\splatterhouse\cache\shaders\shareable\{4E4D07F0.rov.d3d12.xpso, 4E4D07F0.xsh}` y los carga al arrancar (el SDK los marca como *"mostly shareable across devices"*). Para distribuirla: jugar una vez para generarla, **`tools\pack_shadercache.bat`** copia la caché a **`shadercache\`** en el release; `install.bat` los copia a la carpeta del usuario. Si una GPU/driver diera problemas, borrar la caché la regenera.
- **Compilación de shaders/PSO**: el SDK **ya** hace la traducción Xenon→DXBC y la creación de PSO en **hilos de fondo**, **salta el draw** si la pipeline no está lista (`async_shader_compilation=true`) y **precrea en paralelo** todas las pipelines de la caché al arrancar (log `Created N graphics pipelines ... in M ms`). Medido (RTX 5060 Ti): 401 PSOs en ~530 ms; **~2–11 ms por PSO** → el coste es "cientos seguidos", así que la palanca es la **caché completa**, no más async. Diagnóstico: cvar **`sh_pso_log`** (log `[sh-pso] created VS… PS… in X ms` + `draw skipped`). Análisis en `docs/rov-performance.md` §3c.
- **No hay que desencriptar el XEX**: el runtime carga `game:\default.xex` (retail, `XEX2` con flag encrypted) y lo desencripta él mismo (`src/system/xex_module.cpp`, `xe_xex2_retail_key`). Por eso el manifest usa `default.dec.xex` solo en build/codegen.

## Ejecución

```powershell
# cwd OBLIGATORIO: out\build\win-amd64-debug
out\build\win-amd64-debug\splatterhouse.exe     # log -> splatterhouse.log
```

- cvars en `splatterhouse.toml`: `gpu_plugin="xenos"` obligatorio, `log_level=trace`.
- Junction `assets` → `game`. El XEX (`game/default.dec.xex`) está **LZX-comprimido**.
- Variables de instrumentación (ver sección): `REX_DEBUG_CS`, `REX_DEBUG_CS_LEVEL`, `REX_TRACE_LO`, `REX_TRACE_HI`.

## Parches locales del SDK (preservar / reportar upstream)

Diff completo: `C:\Users\josel\AppData\Local\Temp\opencode\sdk_patches.diff` (regenerar con `git -C extern/rexglue-sdk diff > ...`).

Originales (2):
1. `include/rex/platform/exceptions.h` — ajuste SEH.
2. `resources/templates/codegen/pch_h.inja` — **plantilla embebida**: tras editarla, **reconstruir el target `rexglue`** (re-embebe) y regenerar.

> **Eliminado (era el bug del blocker Scaleform):** el antiguo parche 3 en `src/kernel/xboxkrnl/xboxkrnl_io_info.cpp` hacía que `NtQueryInformationFile(XFileXctdCompressionInformation)` devolviera **éxito con estructura a 0** ("no comprimido"). El engine decide si descomprime XCTD según esa consulta (`sub_829F5600` devuelve `2` sólo si la query **falla**; con éxito+0 devuelve `0` y salta la descompresión) → los `.gfx`/`.dds` XCompress (`0F F5 12 ED`) llegaban crudos a Scaleform y el parse fallaba. Se ha **revertido a HEAD** (el SDK ya devuelve `X_STATUS_INVALID_PARAMETER`, igual que Xenia/upstream), lo que restaura la detección por magic `0x0FF512ED`. No hay que re-añadirlo.

Añadidos en esta sesión:
4. `src/kernel/xboxkrnl/xboxkrnl_rtl.cpp` — instrumentación CS `[sh-diag-instr]` (ring buffer, umbral, frames con módulo+RVA) + suavizado de `assert_true` en Debug (no abortar con el diálogo de MSVC). **IMPORTANTE:** el antiguo `[sh-fix]` (bucle de reintento con "REPAIR" que robaba el lock cuando veía `owner==0`) se ha **eliminado**: era la causa de los miles de `RtlLeaveCriticalSection MISMATCH` y de corrupción de memoria (AVs intermitentes). Ahora se usa el **wait bloqueante de Xenia/upstream**. El ABBA original que lo motivó ya estaba resuelto por el fix de codegen r12 (`__finally`).
5. `src/codegen/phase_discover.cpp` — **`dataPointerScan`**: registra funciones apuntadas por dwords en secciones de datos que caen en región de código y **no** dentro de una función ya conocida. Complementa al `VTableScanner` (solo RTTI MSVC; UE3 no lo usa).
6. `src/codegen/builders/context.cpp` + `src/codegen/function_graph.cpp` — **fix de propagación de registros no-ABI** (`r0/r2/r11/r12`): cargas de entrada desde `ctx` y volcado a `ctx` antes de cada llamada. Arregla el bug del `__finally` (r12 = establisher frame) y `__chkstk`.
7. `src/core/seh_win.cpp` — `[sh-av]`: loguea `RIP`+módulo+RVA+dir de fallo en cada access violation.
8. `src/kernel/xboxkrnl/xboxkrnl_io.cpp` — `[sh-io]`: backtrace del llamador de cada open `.gfx`/`.dds` + volcado one-shot de la imagen guest a `guest_image.bin`. **Ahora opt-in** (`REX_SH_IO=1`, antes siempre): 22 líneas por asset a la consola sin buffer stallan el streaming (parones con CPU ~0 %).
9. `include/.../pch_h.inja` — `[sh-trace]`: traza (one-shot por función) de entradas en `[REX_TRACE_LO, REX_TRACE_HI]`.
10. `src/system/xthread.cpp` — logging de `guest_object` en `XThread::Execute`.
11. `src/system/function_dispatcher.cpp` — `[sh-uf]`: backtrace del llamador de cada `Call to invalid or unregistered function` (para encontrar la tabla que calcula el destino); con `REX_SH_UF_CONTINUE=1` loguea y continúa en vez de abortar (modo enumeración).
12. `src/codegen/builders/vector.cpp` + `builders.h` + `src/codegen/instruction_dispatch.cpp` — implementa **`vmaxuw`** (`build_vmaxuw` → `simde_mm_max_epu32`, estaba como `REX_UNIMPLEMENTED`). Es una instrucción de codegen, así que tras tocarla hay que **reconstruir el target `rexglue`** (rexglued.exe) y regenerar (el codegen ya no avisa de `vmaxuw`).
13. `src/graphics/pipeline/shader/translator.cpp` — `[sh-gpu]`: suaviza los asserts de `GatherVertexFetchInformation` (`assert_not_zero(stride)` y el de stride inconsistente) a un `warn`; hay `vfetch` con stride 0 legítimos y en Release el assert estaba fuera. Se compila en **`rexgpu-xenosd.dll`** (plugin GPU), no en `rexruntimed.dll`. Los `warn` ahora son **opt-in** (`REX_SH_GPU=1`) para no inundar la consola en gameplay.
14. `src/kernel/xboxkrnl/xboxkrnl_io_info.cpp` — `XFileXctdCompressionInformation` sigue devolviendo `X_STATUS_INVALID_PARAMETER` (necesario para que el engine descomprima XCTD), pero el log pasa de `ERROR` (inundaba) a `DEBUG` one-shot.
15. `src/graphics/d3d12/pipeline_cache.cpp` — cvar **`sh_pso_async`** (default on): `PipelineCache::EndSubmission()` ya no crea las pipelines en el hilo del present ni espera a los hilos de fondo (era el stall de hasta 673 ms); solo los despierta. Con off, comportamiento original.
16. `src/graphics/d3d12/command_processor.cpp` + `src/ui/d3d12/d3d12_presenter.cpp` — instrumentación **`[sh-frame]`** (cvars `sh_frame_log`/`sh_frame_log_ms`): scopes de tiempo en las fases del present/submission (ver diagnóstico en la sección de rendimiento). Opt-in.
17. `src/core/cvar.cpp` — **`SaveConfig` con merge**: antes serializaba solo los flags != default y **reescribía el fichero entero**, borrando claves no registradas en ese momento (p.ej. cvars del plugin GPU o escritas a mano) → el menú F5 "no guardaba" (perdía opciones anteriores). Ahora parsea el toml existente, actualiza/inserta los flags actuales y **preserva el resto**.
18. `src/graphics/graphics_system.cpp` — **vblank del guest a 60 Hz siempre** (antes con `vsync=false` iba a ~1 kHz y el juego corría acelerado). Cvar `sh_no_vsync_vblank_fast` restaura el modo antiguo.
19. `src/system/xam/content_manager.cpp` + `include/.../content_manager.h` — **`InstallContentFromDirectory`**: escanea una carpeta buscando paquetes STFS (`CON `/`LIVE`/`PIRS`) y los instala con el `InstallContent` ya existente (extracción + `.header` + license mask). Se llama desde el port al arrancar.
20. `src/splatterhouse_app.h` — **`InstallDlcPackages()`** (en `OnPostLoadXexImage`): instala los DLC que el usuario deja en `Documents\splatterhouse\<title>\00000002`, `…\0000000000000000\<title>\00000002` o cualquier `<xuid>\<title>\00000002`.
21. `src/kernel/xam/xam_content.cpp` — **`[sh-dlc]`** opt-in (`REX_SH_DLC=1`): loguea qué contenido enumera/abre el juego (`XamContentCreateEnumerator`/`XamContentCreate`).

(`thirdparty/libmspack` es un submódulo dirty preexistente.)

## Estado y diagnóstico

Cadena original `main`→crash resuelta; el init del **UI/Scaleform GFx** (atlas XCTD) y la **sincronización de critical sections** **también**. El juego carga el atlas completo, arranca el UI, llega a la **cinemática** y se mantiene **>2.5 min** sin AV ni FATAL ni `MISMATCH`.

### Resuelto

1. **Interbloqueo ABBA de CS** (`0x40000618`): era un **ratchet por un `RtlLeave` perdido**, causado por un **bug de codegen**: el handler `__finally` `sub_822379F8` leía el flag de lock desde `[ctx.r31+96]` con `ctx.r31 = r12 - 320`, pero `r12` (establisher frame, no-ABI) **no se propagaba a `ctx`** → leía basura → saltaba el `RtlLeaveCriticalSection`. Corregido en el SDK (punto 6). Verificado: 0 flushes de ratchet, sin `lock_count=90`.
2. **Funciones alcanzadas solo por llamada indirecta** no descubiertas (`Call to invalid or unregistered function`): resuelto con `dataPointerScan` (punto 5). Config `[functions]` en `config/codegen.toml` sigue disponible para casos sueltos.
3. **Registros no-ABI `r0/r2/r11/r12`** no propagados entre llamadas (`__chkstk` tomaba tamaño 0): corregido (punto 6). Auditoría estática (lectura-antes-escritura) → 0 casos residuales en esos registros.
4. **`assert_true` del CS en Debug** (`xboxkrnl_rtl.cpp:599`, `cs->owning_thread == 0`): diálogo modal de MSVC al arrancar el UI. Era una carrera de contabilidad del lock (en Release el assert está fuera y el código roba el lock). Suavizado en el SDK (punto 4).
5. **`Call to invalid or unregistered function`** tras el selector de almacenamiento (`XamShowDeviceSelectorUI`): el análisis de codegen **fusionó funciones contiguas** y las entradas secundarias (thunks `lwz;lwz;mtctr;bctr` / `b tgt` con padding) quedaban inalcanzables. Se añadieron **63 direcciones** a `[functions]` de `config/codegen.toml` (detectadas con `C:\Users\josel\AppData\Local\Temp\opencode\scan_missing3.py`; lista en `config/missing_functions.txt`). El codegen avisa de 3 `bdz ... branches outside function` (0x8235B878/0x8235B888) — revisar si esa zona se usa.
6. **`REX_UNIMPLEMENTED: vmaxuw`** (`splatterhouse_recomp.96.cpp:17341` y `recomp.137.cpp:18886`): instrucción VMX de máximo sin signo no implementada en el codegen. Implementado `build_vmaxuw` (punto 12). **No tiene relación con el `MISMATCH` de CS** (son problemas independientes).
7. **Assert de vertex fetch en la cinemática** (`translator.cpp:411`, `assert_not_zero(fetch_instr.attributes.stride)`): suavizado (punto 13). Se ejecuta en `rexgpu-xenosd.dll`.
8. **`RtlLeaveCriticalSection MISMATCH cs=40651480` + AV intermitentes**: **causa raíz** el bucle `[sh-fix]` "REPAIR" (punto 4). Robaba el lock a un dueño legítimo al muestrear la ventana transitoria `owner=0`/`lock_count` de `RtlLeaveCriticalSection`; el dueño luego desincronizaba `lock_count`/`owning_thread` → MISMATCH en cascada y corrupción (AV al leer punteros basura 0x5F7264 / 0x0700002C). Revertido al wait bloqueante de Xenia: **0 MISMATCH, 0 AV**.
9. **`Call to invalid or unregistered function` en la tabla de callbacks del UI** (`0x825CB158` al llegar al menú/journal, `0x825CB170` en gameplay): son thunks de despacho virtual (`lwz;mr;lwz;lwz;mtctr;bctr`) fusionados por el análisis. Añadida la **tabla contigua completa `0x825CB108..0x825CB1B8`** a `[functions]` de `config/codegen.toml`. Verificado: 3 corridas de 60 s sin AV.

### Resuelto: 60 FPS (parche oficial) + FSR

- El juego venía clavado a **30 FPS**. Con `vsync=false` alcanza **300-500 FPS** → **no hay cap duro**: el 30 era **GPU-bound** (ruta ROV) + presentación sincronizada a vblank. CPU ~2%.
- **Parche oficial de Xenia** (title `4E4D07F0`, `xenia-canary/game-patches`, autor *illusion*), aplicado **durable** (NO tocando `generated/`):
  - **Datos**: floats `0x82F8FF58/5C` (1/15 y 1/30) → **1/60**. En `src/splatterhouse_app.h::ApplyGuestMemoryPatches` (`OnPostLoadXexImage`), activado por la cvar **`sh_60fps`** (on por defecto; definida en `src/main.cpp`).
  - **Instrucción** `0x8247A4AC`: `stb r30,202(r31)` → `stb r11,202(r31)`. Replicado con un **override fuerte de la función weak del codegen** en `src/sh_60fps_hook.cpp` (define `sub_8247A368`, llama a `__imp__sub_8247A368` y fija `[obj+202]=arg`); añadido a `CMakeLists.txt`.
  - **Ambas son necesarias**: solo los floats → sigue a 30.
- **FSR** activado (ver Build). Con `present_effect="fsr"` + `video_mode 1920x1080` + fullscreen + vsync, el gameplay sostiene **60 FPS** (estado estacionario 100% ≥55; los 30 son carga/intro). `bilinear` da FPS similar (FSR es sobre todo **calidad** del reescalado); FSR2/3 son temporales **experimentales** (motion/depth sintetizados, pueden caer a FSR espacial). **DLSS/XeSS no son viables** (UE3 no expone motion vectors).
- Diagnóstico: cvar **`sh_log_fps`** (log `[sh-fps] guest N FPS` cada 2 s) y log `[sh-fps] vblank pacing`, en el plugin GPU (`rexgpu-xenosrd.dll`).
- **VSync OFF + present frame limiter (default)**: un frame que excede 16.6 ms hace que el vsync baje a **30** (mitad de vblank) → tirones. Por eso el port usa **`vsync=false` + `d3d12_present_frame_limiter=true` + `d3d12_present_frame_limiter_fps=60`**: la petición de present **bloquea** hasta el turno del limiter, así el guest va a 60 sin caer a 30. **Gotcha**: `vsync` y `d3d12_present_frame_limiter*` los registra el **plugin GPU**, que se carga **después** de `OnPreSetup` → allí `SetFlagByName` es un no-op. Los default se fijan en **`OnPostSetup`** (`src/splatterhouse_app.h`), que corre con el plugin ya cargado (y el worker de vblank lee `vsync` en vivo), o directamente en el `splatterhouse.toml` del instalador. Los logs `[sh-perf]`/`[sh-fps] vblank pacing` confirman los valores.
- **Tearing**: con `vsync=false` el present usa `Present(0, ALLOW_TEARING)` → podía desgarrar en cualquier frame (menús/carga/cinemáticas/gameplay). Default **`d3d12_allow_variable_refresh_rate_and_tearing=false`** (opción "flip sin tearing", sin volver al bajón a 30 del vsync clásico). Alternativa si el monitor tiene VRR: dejarlo `true` y activar G-Sync/FreeSync.
- **Aceleración ("cámara rápida") — RESUELTO**: el cvar **`vsync` estaba sobrecargado** y controlaba a la vez el present del host **y** la cadencia del **vblank del guest**. Con `vsync=false` el worker de vblank emitía vblanks a **~1000 Hz** (`guest_tick_frequency/1000`) en lugar de 60 Hz → los juegos que ligan el tiempo al vblank corrían **acelerados (×~16)**. Fix en `graphics_system.cpp`: el vblank del guest va **siempre** a la tasa del video mode (60 Hz), independientemente de `vsync`. Cvar **`sh_no_vsync_vblank_fast`** (default off) restaura el comportamiento antiguo. **No** tenía relación con `sh_60fps`.
- **Medición definitiva (gameplay forzado con `sh_warp=lvl2_shanty`, corrida >2 min)**: con `vsync=false`+limiter el guest **presenta a ~60 VdSwap/s** de forma estable (`[sh-swap]`, instrumentación en `VdSwap`). CPU **1–5 % de 12 núcleos**; **GPU 3D 60–89 %**. Conclusión: **el port es GPU-bound (ruta ROV)**, no CPU-bound → repartir trabajo entre núcleos no ayuda. Los "parones" son momentos en que un frame **excede 16.6 ms** (GPU) y cae a **30** (mitad de vblank/limiter). Palancas reales: bajar `video_mode_*` (preset F5), abaratar ROV, o **`fullscreen=true`** (el `sh_warp`/arranque en ventana da menos FPS).
- Nota: la cvar `video_mode_refresh_rate` (double con `.allowed`) **rechaza `"120.0"`** — usar `"120"`.

### Añadido: pantalla de gráficos in-game (F5)

- **No se puede editar el menú real**: el UI es **Scaleform GFx** y los `.gfx` son Flash compilado + **XCompress** (`0F F5 12 ED`), sin fuentes `.fla`; el SDK no trae descompresor XCompress offline (`XMemDecompress` lo implementa el engine guest). El layout vive dentro del `.gfx`.
- **Solución (opción A)**: pantalla propia a **pantalla completa con estilo de juego** (fondo oscuro + rojo), NO una ventana flotante. `src/graphics_menu.h` (`GraphicsMenuDialog : rex::ui::ImGuiDialog`), abierta/cerrada por el keybind **`bind_graphics`** (F5 por defecto). Se integra en `src/splatterhouse_app.h::OnCreateDialogs`. El diálogo se registra en el `ImGuiDrawer` y se auto-borra con `Close()`; el puntero global `g_graphics_menu` evita dangling.
- **F5 (importante)**: el keybind **no se disparaba** porque el `ImGuiDrawer` se registra como input listener con **z-order 64** y consume la tecla antes. Solución: un **`WindowInputListener` propio con z-order 128** (`SplatterhouseApp::GraphicsKeyListener`, añadido en `OnCreateDialogs` con `window()->AddInputListener(..., 128)` y quitado en `OnShutdown`) que maneja F5 antes que ImGui. `ReXApp::OnKeyDown` es **privado** → no se puede sobrescribir.
- **Opciones** (leen/escriben cvars; `S` = guardar en `splatterhouse.toml`, Esc/F5 = cerrar): **Preset** (Bajo 960x540 / Medio 1280x720 / Alto 1600x900 / Ultra 1920x1080; fija `video_mode_*`, `present_effect="fsr"` y `texture_cache_memory_limit_soft`), Presentación (`present_effect`), Resolución interna, Ruta de render (`render_target_path_d3d12`), VSync, Límite de FPS (`d3d12_present_frame_limiter[_fps]`), Nitidez FSR, Dither, Escala de dibujo (`resolution_scale`). Marca `[reinicio]` en las que lo requieren. Cambiar cualquier fila pasa el Preset a "Personalizado".
- **Nota de layout**: la columna de valores se coloca en `panel_x + panel_w*0.58` para no cortar el `[reinicio]`; en **windowed** con `window_width` mayor que el escritorio el overlay puede quedar recortado (usar fullscreen o `window_width=0`).
- **Pendiente**: engancharla al **menú de pausa del juego** (hoy se abre con tecla). Requeriría RE del estado de pausa.
- Diagnóstico: cvar `sh_graphics_menu` (si true, la abre al arrancar; útil para capturas).

### Resuelto: localización (texto) con audio siempre en inglés

- El juego (Gamebryo) obtiene el nombre del idioma con **`sub_827B65E0(this)`** = `tabla[this+120]` (tabla guest **`0x82F90110`** = `{English,French,Italian,German,Spanish,Japanese}`) y construye `data/localization/%s.xml`. **No** usa `XGetLanguage`.
- Esa función tiene **dos** call sites: **`sub_82E786E8`** (arranque temprano/audio) y **`sub_8277CFC8`** (`LocalizationManager::LoadLanguage`). Forzar el idioma en **ambos** crashea (strlen sobre puntero malo en `sub_829C3AF0`) — el copy solo está doblado a English(US).
- **Solución** (`src/sh_language_hook.cpp`): override weak de `sub_827B65E0` que **solo** cambia el idioma cuando la llamada viene de `sub_8277CFC8`, distinguido por el **return address del host** (`__builtin_return_address(0)` vs `&sub_8277CFC8`/`&sub_82E786E8`). Devuelve el nombre en **minúsculas** (los ficheros son `spanish.xml`). El arranque/audio queda en English → **texto localizado, audio inglés, sin crash** (verificado: menú en español).
- Cvar **`sh_language`** (`auto` = locale del host, `default` = original, o `english/french/italian/german/spanish/japanese`); fila **Idioma** en la pantalla F5. También cvar `user_language` en el SDK para `XGetLanguage`.

### Resuelto: desbloquear niveles/capítulos (NO es "Global Conditions")

- **La hipótesis de "Global Conditions" era incorrecta**: el save (`GameSave.sav`, **plaintext**) solo guarda condiciones de skills/collectibles/arenas, no capítulos. El desbloqueo real es la **progresión de nivel** del `GameSaveManager`.
- **Mecanismo** (verificado con instrumentación runtime): el frontend Scaleform pregunta `GetHighestLevelCompleted`. Se resuelve en el getter de stats por nombre **`sub_82E5C540(this, name)`** (`generated/splatterhouse_recomp.81.cpp:37020`), que para ese nombre devuelve **`[statsObj+392]`** como **`char*`** (`r3`); el handler AS que lo llama es `sub_82E21EF8` (registrado como `GetStat` en `sub_82F044C8`/`recomp.84.cpp:39024`). Fuente alternativa: `GetHighestReachedLevel` → handler `sub_82F037B8` → **`sub_82EF0FE0(saveObj)`** (`recomp.161.cpp:37663`), que recorre el array `[saveObj+16]` (el tamaño sale del manager de dificultades en `0x831A7570`; **count=3 = dificultades, NO niveles**).
- **Solución** (`src/sh_unlock_hook.cpp`): overrides weak de **`sub_82E5C540`** (devuelve `sh_unlock_levels_max`, por defecto **7**, como string en buffer guest) y de **`sub_82EF0FE0`** (devuelve el máximo). Cvar **`sh_unlock_levels`** (on por defecto) + **`sh_unlock_levels_max`**; fila **Desbloquear niveles** en la pantalla F5. Cvar **`sh_unlock_debug`** (one-shot) loguea las consultas. Verificado: `GetHighestLevelCompleted original=0 -> 7`, sin AV.
- **Nota**: los directorios de nivel son `lvl1..lvl5` y `lvl7` (no hay `lvl6`), pero el **selector muestra 8 capítulos** (confirmado por el usuario). Con `sh_unlock_levels_max=7` salen todos: la condición de desbloqueo es `capítulo <= completado+1`, así que 7 basta para el 8º. Si alguno quedara bloqueado, subir el cvar (p. ej. 8).

### Resuelto: DLC (Survival Arenas, máscaras, etc.)

- **Sí se pueden jugar los DLC** si el usuario tiene los paquetes. La cadena: en X360 el DLC era un **paquete STFS** (`CON `/`LIVE`/`PIRS`) que la consola montaba como `XContent` con un `info.xml` que declara niveles (`DownloadPackage` → `<Level><Flag name="Is DLC" val="1"/>`). El engine (`DownloadManagerXenon` → `LevelDatabase::AddDLCLevel`) enumera/abre el contenido y añade los niveles al `leveldb`.
- **El runtime ya implementaba** `XamContent*` (`ContentManager`: enumerar/crear/abrir/montar + license mask) y monta el paquete como device VFS; el engine lee el `info.xml` del paquete montado. **Faltaba instalar** los paquetes.
- **Implementado**: `SplatterhouseApp::InstallDlcPackages()` (`OnPostLoadXexImage`) busca STFS en `Documents\splatterhouse\<title>\00000002`, `…\0000000000000000\<title>\00000002` y **cualquier `<xuid>\<title>\00000002`**; `ContentManager::InstallContentFromDirectory` (nuevo) los detecta por magic y los instala (`InstallContent`: extrae a `…/0000000000000000/<title>/00000002/<file>/` + escribe `.header` + license mask). Idempotente.
- **Verificado**: con 6 paquetes en `B13EBABEBABEBABE\4E4D07F0\00000002`: se instalan los 6; el juego hace `XamContentCreateEnumerator type=00000002 -> 6 item(s)` y `XamContentCreate type=00000002` (abre el paquete); **el usuario confirma ver los DLC y las máscaras en el menú**.
- **`XamContentOpenFile` es un stub** en el SDK (devuelve not-found) pero **no hace falta**: el engine lee los ficheros del paquete por el VFS montado.
- Diagnóstico: **`REX_SH_DLC=1`** (opt-in) loguea la enumeración/apertura. Doc completa en **`docs/dlc.md`**.

### Herramienta dev: warp de nivel (`sh_warp`)

- Para **calentar la caché de shaders por capítulos** sin jugarlos: cvar **`sh_warp`** (nombre de nivel) → el runtime carga ese nivel. Fila **Warp nivel (dev)** en el menú F5 (cicla `lvl1_manor_interior`, `lvl2_shanty`, `lvl3_manor_catacombs`, `lvl4_carnival`, `lvl5_manor_grounds`, `lvl7_manor_chapel`, `survival_arena`, `frontend`).
- Implementado en `VdSwap_entry` (SDK, `xboxkrnl_video.cpp`): lee `sh_warp`, obtiene el `GameState` global (`*(0x82FFA3AC)`) y llama a `GameState::RequestLoadLevelByLevelName` (`0x82795E48`, `r3=GameState`, `r4=nombre`) vía `FunctionDispatcher::Execute` (hilo guest; reintenta hasta que el `GameState` exista y limpia la cvar). **Específico de Splatterhouse.** Verificado: dispara sin crash.
- Flujo: in-game/menú → **F5** → **Warp nivel** → elegir nivel → esperar a que cargue → repetir. Luego `tools\pack_shadercache.bat`.

### (histórico) Localización — primer intento, bloqueado

- El juego usa **Gamebryo**: el idioma sale de la clave **`LanguageDef`** de su config (`game/data/options.xml`, **XCompress**); **NO** usa `XGetLanguage`/`XamGetLocale` (verificado con logs).
- El nombre del idioma lo devuelve **`sub_827B65E0(this)`** = `tabla[this+120]`, con la tabla en datos guest **`0x82F90110`** = `{English, French, Italian, German, Spanish, Japanese}` (punteros a nombre). `LocalizationManager::LoadLanguage` construye `%s\%s.xml` y cae a `english.xml` si no lo encuentra.
- **Implementado** (`src/sh_language_hook.cpp`): override weak de `sub_827B65E0` que devuelve el idioma de la cvar **`sh_language`** (`auto` = locale del host vía `GetUserDefaultUILanguage`; `default` = comportamiento original). Añadida fila **Idioma** a la pantalla F5. También cvar **`user_language`** en el SDK para `XGetLanguage` (útil para juegos que sí lo usan).
- **Bloqueo**: este copy solo trae bancos de audio **`English(US)`** (`D:\Data\sounds\soundbanks\XBox360\English(US)\`). Forzar otro idioma hace buscar bancos inexistentes → **crash** (wild pointer). El **texto** sí se localizaría (`data/localization/<lang>.xml` existen); falta el audio multiidioma (versión EU) o un fallback. Dejado `sh_language = "default"` (estable).
- Pendiente: fallback de audio (usar English(US) si falta el idioma) para poder localizar solo el texto.

### Resuelto: blocker Scaleform/XCTD (atlas `.dds`)

- El AV `sub_82823E28` (`generated/splatterhouse_recomp.86.cpp:18045`) era un **síntoma**: `[singleton+520]==0` porque el movie `subtitles.gfx` no cargaba. Cadena: `sub_828249C0` → `sub_828245F8` → vtable+44 = `sub_828419C8` → `sub_825ED970` → `sub_825F0AD0` → `sub_82604518` → `sub_82609F50` (loader Scaleform).
- **Causa raíz**: assets `.gfx`/`.dds` comprimidos con **Xbox 360 XCompress** (firma `0F F5 12 ED`, `XMemDecompressSegmentTD`). El engine (`sub_829F5E70` → `sub_829F5668`) los descomprime de forma transparente, pero para ello consulta `NtQueryInformationFile(XFileXctdCompressionInformation)` (`sub_829F5600`, clase `27`); **sólo si la consulta falla** cae al path de detección por magic. El parche local 3 devolvía éxito+0 → el engine leía crudo → Scaleform no parseaba → `+520==0` → AV. Revertir el parche 3 (el SDK ya devuelve `INVALID_PARAMETER`, como Xenia) lo arregla.
- **Verificado**: 0 access violations; se abre el atlas UI completo (**243+ `.dds`**: `savingicon.dds`, `bloodspiral01.dds`, `errorscreen_i*.dds`, …), `sh_menu_01.gfx`, `loadingscreen.gfx`; el juego renderiza y se mantiene >20 s.
- Nota: `REX_TRACE`/`sh-io` siguen disponibles; el descompresor XCTD vive en el engine (`sub_829F2EB8`/`sub_829F4B58`, detección en `sub_829F5668`/`sub_829F5600`).

### Instrumentación (env vars)

| var | efecto |
|---|---|
| `REX_DEBUG_CS=40000618` | vigila ese CS (entero/leave + frames) |
| `REX_DEBUG_CS_LEVEL=4` | umbral de recursión para volcar (default 4) |
| `REX_TRACE_LO=82800000` `REX_TRACE_HI=82A00000` | traza (1 vez/función) las entradas de función en ese rango |
| `[sh-av]` (siempre) | PC+RVA del access violation |
| `[sh-swap]` (con `sh_log_fps`) | tasa real de `VdSwap/s` (present del guest) — instrumentado en `VdSwap_entry` |
| `REX_SH_IO=1` | backtrace del llamador de opens `.gfx`/`.dds`; vuelca `guest_image.bin` (19.6 MB). **Opt-in**: era el `[sh-io]`, que escribía 22 líneas **por asset** a la consola sin buffer (stall de I/O en el streaming), así que ahora está apagado por defecto |
| `REX_SH_GPU=1` | avisos `[sh-gpu]` de `vfetch` (stride 0 / mismatch). **Opt-in** por el mismo motivo |
| `REX_SH_DLC=1` | `[sh-dlc]`: qué contenido enumera/abre el juego (`XamContentCreateEnumerator`/`XamContentCreate`). Opt-in |

### Pendiente (prioridad: crashes > rendimiento > gráficos)

1. **Crashes restantes**: ya se entra en gameplay (se golpea a enemigos). El `Call to invalid or unregistered function` de turno se resuelve añadiendo el target a `[functions]` (el `[sh-uf]` da la dirección; `scan_missing4.py` enumera la familia `mtctr;bctr`). **Lección aprendida**: añadir los candidatos **en bloque no es seguro** — los thunks aislados que devuelve el detector estricto pueden partir funciones de init y colar un **AV determinista a los ~6 s** (probado: lote de 108 → AV en `sub_824C1558`; solo la **tabla contigua** `0x825CB108..0x825CB1B8` es segura). Regla: añadir de a pocos y verificar 3 corridas de 60 s sin AV.
2. **Rendimiento**: mitigado (60 FPS con el parche `4E4D07F0` + FSR a 1080p). Para GPUs débiles: **presets** (Bajo/Medio/Alto/Ultra) de la pantalla F5. La optimización de fondo es abaratar la ruta **ROV**; análisis y plan en **`docs/rov-performance.md`** (el TODO del SDK es especializar el pixel shader con los parámetros de RT estáticos).
   - **Diagnóstico de "parones" (medido, >2 min de gameplay real)**: es **GPU-bound**. CPU 1–5 % de 12 núcleos; **GPU 3D 60–89 %**. El guest presenta a ~60 `VdSwap/s` (`[sh-swap]`, log añadido a `VdSwap` en el SDK); los tirones son frames >16.6 ms que caen a **30** (mitad de vblank/limiter). Repartir entre núcleos **no ayuda** (no hay carga de CPU). Palancas: bajar `video_mode_*` (preset F5), `fullscreen=true`, o abaratar ROV.
   - **Causa raíz del parón grande (RESUELTA): `PipelineCache::EndSubmission()`**. Aunque `async_shader_compilation=true` salta el draw, `EndSubmission` **creaba las pipelines encoladas en el hilo del present** (`CreateQueuedPipelinesOnProcessorThread`) y **esperaba a los hilos de fondo** (`Wait`) → stall de hasta **673 ms** en el hilo de UI durante el present (al entrar en zonas con materiales/PSO nuevos). **Fix**: cvar **`sh_pso_async`** (default **on`) → `EndSubmission` solo despierta a los hilos de fondo y **no espera**; los draws sin pipeline se saltan (popping). Verificado: `EndSubmission(swap)` 40.9→4.1 ms; frame máx 673→51 ms. Reversible con `sh_pso_async=false`.
   - **Instrumentación de diagnóstico** (opt-in, cvar `sh_frame_log` + `sh_frame_log_ms`): scopes `[sh-frame]` en `IssueSwap`, `Begin/EndSubmission` (por fase: `pipeline_cache_EndSubmission`, `SubmitBarriers`, `allocator_Reset`, `Signal`, `deferred_execute`, `ExecuteCommandLists`), `CheckSubmissionFence/wait-GPU`, `IssueDraw`, `IssueCopy`, y en el presenter (`PaintAndPresentImpl`, `frame_limiter_wait`, `swapchain_Present`, `RefreshGuestOutputImpl`, `refresher-lambda`, host presents/s). **Ojo**: `host PaintAndPresent` mide ~33 ms pero **32.5 ms son la espera voluntaria del frame limiter** (no trabajo); el render real es <1 ms y el host presenta a **60/s**. No confundir con un stall.
   - **Nota "camara rapida" (x2)**: con `vsync=false` el juego **no** se acelera (el guest sigue presentando a 60); lo que era x2 era el **parche 60 FPS aplicado sobre una base de 30**. Verificar `sh_60fps` si se vuelve a ver.
   - La traducción de shaders y la creación de PSO **ya** van en hilos de fondo (`d3d12_pipeline_creation_threads` = núcleos×3/4) y la caché se precrea al arrancar (401→1072 PSOs en <0.3 s; **0** durante el juego). `async_shader_compilation=true` salta el draw hasta que la pipeline está lista (popping, no congelación).
   - Se pasó la instrumentación pesada a **opt-in** (`REX_SH_IO`/`REX_SH_GPU`): el `[sh-io]` escribía un backtrace de 22 líneas a la consola **sin buffer** en cada open `.dds`/`.gfx` (stall en el path de streaming). Reconstruidos `rexruntimerd.dll` + `rexgpu-xenosrd.dll`.
   - Si los tirones siguen: el frame que excede 16.6 ms hace que el **vsync baje a 30** (mitad de vblank). Palancas: bajar `video_mode_*` (preset), `vsync=false`+`d3d12_present_frame_limiter`, o abaratar ROV. Revisar por qué la carga/intro arranca a 30 antes de estabilizar a 60.
3. **Bugs gráficos (iluminación) — RTV no es arreglable por config**: escena 3D casi negra, solo emisivos/lámparas y UI visibles (silueta) con **RTV**. Es la **limitación de diseño** de la ruta RTV (confirmado en la wiki de Xenia y el issue PGR4 #161): la eDRAM del X360 se reinterpreta en varios formatos y tiene formatos inexistentes en PC (7e3 HDR, fixed −32..32…); la ruta RT renderiza a texturas PC y **aproxima/copia**, y pierde el solape/reinterpretación de eDRAM que usan los pases de iluminación. **ROV renderiza directo a la eDRAM** (blending/depth en el shader) y lo reproduce bien.
   - **Obligatorio** `render_target_path_d3d12 = "rov"` (por defecto en GPU no-Intel es `rtv` → escena negra).
   - **Probado y NO arregla** (RTV + `readback_resolve="full"`, `direct_host_resolve=false`, `mrt_edram_used_range_clamp_to_min=false`, `gamma_render_target_as_unorm16=false` → sigue oscuro). Arreglarlo de verdad = reimplementar en la ruta RTV lo que hace ROV → proyecto grande de gráficos.
   - Con **ROV + FSR** el gameplay ya sostiene **60 FPS** a 1080p, así que RTV no hace falta para el rendimiento.
4. **Post-procesado/DoF-blur** excesivo al inicio: probablemente DoF low-res de UE3 + upscale 720p→ventana; mitigable con `resolution_scale = 2` (coste GPU).
4. Revisar los 3 avisos de codegen `bdz ... branches outside function` (0x8235B878/0x8235B888) — heurísticos.
5. Tooling Xenia: sigue en `C:\Users\josel\Downloads\xenia_canary_windows` (log verbose; `trace_functions`+`trace_dump_path`).

## Auditoría estática útil

- Fichero: `C:\Users\josel\AppData\Local\Temp\opencode\scan_rbw.py` — lee-antes-de-escribir de registros locales sobre `generated/*.cpp` (filtra spills de LR y helpers `__save*`/`__rest*`).
- Fichero: `C:\Users\josel\AppData\Local\Temp\opencode\find_addr.py` — localiza referencias `lis/addi` a una dirección guest.
- Nota: los miles de avisos `r30/r31` son **spill muerto** del prólogo; los 15 "sin terminador" son **inofensivos** (return implícito del host).

## Convenciones

- NO añadir comentarios al código salvo que se pida.
- No commitear nunca sin instrucción explícita. No commitear assets ni secretos (`game/` está gitignored).
- El runtime propio antiguo vive en `legacy/` (solo referencia, no compila con el stack nuevo).
