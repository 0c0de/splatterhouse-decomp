# ROV: por qué es necesario y cómo optimizarlo

Documento de rendimiento del port. Resume el análisis de la ruta **ROV**
(Rasterizer-Ordered Views / Pixel Shader Interlock) del backend D3D12 de
ReXGlue SDK y las vías para acelerarla, pensado para GPUs modestas.

## 1. Por qué ROV es obligatorio (y no RTV)

El Xbox 360 escribe en una **eDRAM de 10 MB** que los juegos **reinterpretan**
constantemente en distintos formatos (clear, HDR, ping-pong de iluminación…), y
usa formatos que **no existen en PC** (7e3 HDR, fixed −32…32, depth 20e4). En PC
los render targets son recursos independientes.

- Ruta **RTV** (RT/DSV + copias host↔eDRAM): rápida, pero **aproxima** formato y
  reinterpretación → en juegos que solapan/reinterpretan eDRAM (p.ej. la
  iluminación de Splatterhouse, y PGR4 — ver Xenia issue #161) la escena queda
  casi negra (solo emisivos/UI). **No se arregla con cvars** (probado:
  `readback_resolve=full`, `direct_host_resolve=false`,
  `mrt_edram_used_range_clamp_to_min=false`, `gamma_render_target_as_unorm16=false`
  → sigue oscura).
- Ruta **ROV / FSI**: renderiza **directo** a la eDRAM (un buffer `R32_UINT`) y hace
  blending / depth / stencil **a mano en el pixel shader**. Correcta, pero más
  lenta.

En Vulkan la disyuntiva es idéntica: `render_target_path_vulkan = "fbo"` (como
RTV) vs `"fsi"` (como ROV, "much slower"). No cambia el problema.

**Conclusión:** ROV es la única ruta correcta hoy. Todo el trabajo de rendimiento
para GPUs débiles pasa por **abaratarla** o por **reducir píxeles**.

## 2. Por qué ROV es lenta

El pixel shader de ROV (`extern/rexglue-sdk/src/graphics/pipeline/shader/dxbc_translator_om.cpp`,
~3000 líneas) emula el output-merger **en software**, con estado leído del
**system-constant buffer** y **ramificado por píxel**:

| system constant | uso |
|---|---|
| `xe_edram_rt_format_flags` | des/empaquetado de formato (`OpSwitch`) |
| `xe_edram_rt_blend_factors_ops` | factores/operaciones de blend (`OpSwitch`/`OpIf`) |
| `xe_edram_rt_keep_mask` | máscaras de escritura por componente |
| `xe_edram_rt_clamp` | clamp por formato |
| `xe_edram_rt_base_dwords_scaled`, `xe_edram_32bpp_tile_pitch_dwords_scaled` | direccionamiento eDRAM por tiles |
| `xe_edram_stencil`, `xe_edram_blend_constant`, sample count | stencil/blend/msaa |

Cada píxel además hace **read-modify-write** de la eDRAM (UAV) con dirección de
tile calculada. Resultado: mucha instrucción por píxel, **divergencia** y
dependencia de la eDRAM → el cuello típico en GPUs modestas.

## 3. Optimización que apunta el SDK: especialización estática

TODO del SDK (`extern/rexglue-sdk/src/graphics/d3d12/render_target_cache.cpp`,
selección de `render_target_path_d3d12`):

> *"Make ROV the default when it's optimized better (for instance, using **static
> shader modifications to pass render target parameters**)."*

Los parámetros del render target (formato, blend, write masks, clamp) son
**constantes por draw** (vienen del register file). Hoy el traductor **no** los
recibe al compilar (el constructor de `DxbcShaderTranslator` solo recibe
`edram_rov_used`, MSAA, escala…), así que el shader es genérico y los `OpIf`/
`OpSwitch` se evalúan en runtime.

**Plan (upstream, gran alcance):**
1. Pasar la config de RT (color formats, blend control/ops, write masks, clamp)
   al `DxbcShaderTranslator` / `SpirvShaderTranslator` al compilar.
2. Sustituir los `LoadSystemConstant(kEdramRTFormatFlags/…)` del OM por esos
   valores **estáticos** → constant folding; los `OpSwitch`/`OpIf` desaparecen.
3. Incluir esos valores en la **clave del PSO** (variantes cacheadas).
4. Conservar en dinámico lo que de verdad lo es (p.ej. selección de RT por
   control flow: ver `command_processor.cpp`, comentario de "lighting pass").

Coste/riesgo: **alto** (ambos backends, posible regresión de correctitud). Es la
contribución idónea para el SDK.

## 3b. Cota medida (RTX 5060 Ti, 1080p, FSR, vsync off, gameplay)

Comparación ROV vs RTV (OM barato, iluminación rota) para acotar cuánto pesa el
output-merger en software de ROV:

| Ruta | mediana | p90 | máx |
|---|---|---|---|
| ROV  | 83 FPS  | 178 | 302 |
| RTV  | 134 FPS | 354 | 413 |

ROV es ~**1.6×** más lento. Es la **cota superior** de lo recuperable (parte de
la diferencia son copias eDRAM que ROV evita, así que la especialización
recuperaría menos, ~1.2-1.4×). Con vsync a 60 Hz no limita en esta GPU, pero en
GPUs modestas sí.

### Bloqueo de implementación

Especializar requiere meter los parámetros de RT en la **clave del shader**, que
hoy es un `uint64_t` (`DxbcShaderTranslator::Modification::value`,
`shader.h: std::unordered_map<uint64_t, Translation*>`) y **no tiene espacio**
para `edram_rt_format_flags[4]` (128 bits). Hay que **ensanchar la clave** en todo
el SDK: `Modification`, `Shader`/`Translation`, `PipelineDescription`,
serialización `.xpso`/`.xpso`-Vulkan y `Modification::kVersion`. Es un cambio
multi-día (ambos backends) y candidato a PR upstream.

## 3c. Compilación de shaders / PSO (los tirones de la "primera vez")

Al usar un material nuevo, el port: **(1)** traduce el shader Xenon → DXBC (**en hilos de fondo**), **(2)** crea el PSO D3D12 (compilación del driver, **en hilos de fondo** `d3d12_pipeline_creation_threads`) y **(3)** si aún no está listo, **salta el draw** (`async_shader_compilation = true`) → popping breve en vez de congelar el frame. Todo esto **ya está en el SDK**.

Al arrancar, el SDK **precrea en paralelo** todas las pipelines de la caché y lo registra (`log_level=info`):

```
Translated 346 shaders from the storage in 28 milliseconds
Created 401 graphics pipelines (not including reading the descriptions) from the storage in 530 milliseconds
```

**Medición por PSO** (`sh_pso_log=true`, RTX 5060 Ti): **~2–11 ms** cada uno. O sea, no es un PSO carísimo, sino **cientos seguidos** al entrar en una zona nueva → la solución es **caché completa + prewarm** (ya existente), no más asincronía.

- Diagnóstico: cvar **`sh_pso_log`** → log `[sh-pso] created VS … PS … in X ms` por pipeline y `draw skipped` cuando falta uno; `log_level=info` muestra los totales del arranque.
- Reparto: `d3d12_pipeline_creation_threads` (auto).
- **Distribuir la caché**: jugar una vez → `tools\pack_shadercache.bat` copia `%MyDocuments%\splatterhouse\cache\shaders\shareable\*` a `shadercache\` del release; `install.bat` los instala al usuario. Con la caché completa, los PSO se crean **en el arranque** (una vez) y no durante el juego.
- Opcional (per-machine, no distribuible): `ID3D12PipelineLibrary` para guardar los blobs compilados por el driver y que el prewarm sea casi instantáneo en ejecuciones posteriores.

## 3d. Perfil medido del combate (`sh_render_profile`)

Instrumentación opt-in en el command processor D3D12 (log `[sh-render]`): agrega
por intervalo draws, por par VS/PS (hash de ucode), por tipo de primitiva y por
"pase" (formatos de color + base de color0 + pitch de superficie + msaa + base y
formato de profundidad), con un proxy de coste por píxel (área de scissor
acumulada). Se activa con:

```toml
sh_render_profile = true
sh_render_profile_interval_s = 2.0   # cada cuánto vuelca el resumen
sh_render_profile_top = 8            # nº de entradas top a listar
```

Medido (RTX 5060 Ti, 1080p, D3D12+ROV, gameplay real):

| Escena | draws/frame | verts/frame | pases dominantes |
|---|---|---|---|
| Frontend/menú | ~75 | ~0.4k | 1 pase principal (pitch 1040, 2x MSAA) |
| **Combate (3+ enemigos)** | **hasta ~1350** | **hasta ~1.1M** | escena principal (~850 d/f) + **profundidad/sombras** (~475 d/f) |

- Primitivas: dominan las **triangle lists** (`0x4`).
- Shaders del pase de profundidad (VS-sin-PS): `4B7396D6DA4F3D08`, `2EE71F998E3C653F`,
  `8A4840C848499397`, `B6C9863F710683EC`, `007A49CEB83DEFD0`.
- En la RTX 5060 Ti el swap se mantiene ~60 (mín 30); el "slideshow" lo reportan
  GPUs **AMD** (RX 9070 XT / RX 7800 XT), donde el interlock (ROV) es mucho más caro.

**Objetivo**: abaratar el OM software de la ruta ROV en **los dos pases** (el de
color de la escena y el depth-only de sombras), no solo las sombras. En ROV, los
draws sin PS usan `depth_only_pixel_shader_` (`dxbc_translator_om.cpp`,
`CompletePixelShader`), que hace el test/escritura de profundidad en el shader
bajo interlock.

## 4. Wins parciales (más baratos)

- Especializar solo los combos más frecuentes (p.ej. RT `k_8_8_8_8`, 1 RT, sin
  blending) cubre la mayoría de draws.
- Eliminar el cálculo de keep-mask/clamp cuando el formato ocupa el registro
  completo.
- Saltar el des/empaquetado de color cuando el shader no escribe ese RT
  (ya se hace: `shader_writes_color_targets`).

## 5. Palancas sin tocar el shader (lo entregable hoy)

El coste de ROV escala con **píxeles**; en GPUs débiles lo más eficaz es bajar la
resolución interna del guest y reescalar con FSR:

- `video_mode_width/height` ← resolución interna del guest (el mayor lever).
- `present_effect = "fsr"` ← reescala a la ventana con más calidad que bilinear.
- `d3d12_present_frame_limiter[_fps]` ← limitar a 30 FPS si no llega a 60.
- `texture_cache_memory_limit_soft` ← menos VRAM (ayuda en GPUs con poca memoria).

Presets en la pantalla de gráficos del port (tecla **F5**):

| Preset | video_mode | cache texturas |
|---|---|---|
| Bajo  | 960x540  | 256 |
| Medio | 1280x720 | 512 |
| Alto  | 1600x900 | 768 |
| Ultra | 1920x1080| 1024 |

## 6. Cómo medir

- cvar `sh_log_fps` → log `[sh-fps] guest N FPS` cada 2 s (plugin GPU).
- Overlay de debug del runtime (F3) → FPS.

## 7. Reportes de AMD/Intel (issue #3) y qué se puede hacer hoy

Reportes de la issue #3: **RX 9070 XT, RX 7800 XT y Arc B580** — en pasillos van
bien, pero **en combate con ≥2 enemigos va a saltos**; el mismo usuario confirma
que forzando **Vulkan** el combate va fluido.

- **Confirmado**: no es CPU (medido: 1–5 % de 12 núcleos) ni un bug del port. Es
  el **interlock de la ruta ROV en D3D12**, que en AMD/Intel es
  desproporcionadamente caro con **overdraw** (combate: personajes y partículas
  solapándose) y con **dos pases** (color ~850 draws + profundidad/sombras ~475).
  En NVIDIA el mismo workload sostiene 60 FPS.
- **Vulkan (FSI) no es la solución todavía**: es rápido en combate (su interlock
  es nativo), pero hoy renderiza **globalmente más oscuro** y en algunas escenas
  saca **capas negras/opacas**; ademas `present_effect=fsr` no aplica. Queda como
  vía de futuro (arreglar gamma + estabilidad), no como default.
- **RTV no vale**: arreglaría el rendimiento pero **rompe la iluminación** (es la
  limitación de diseño de la ruta RTV, ver §1).

**Mitigación entregada (2026-10)**: el coste de ROV escala con píxeles, así que
la palanca es la **resolución interna**:

| GPU | Resolución interna por defecto del instalador | Píxeles vs 1080p |
|---|---|---|
| AMD / Intel | 1280x720 + FSR | ~0.44x |
| NVIDIA | 1920x1080 + FSR | 1x |

`install.bat` detecta el nombre de la GPU (`Win32_VideoController`) y escribe el
`splatterhouse.toml` con 720p en AMD/Intel. En la pantalla **F5** se puede subir
o bajar en caliente con **Preset** (Bajo 960x540 … Ultra 1920x1080) o con
**Resolucion interna**; el preset conserva el aspecto (16:9, 21:9…).

Si aún no llega: bajar a **960x540** (0.25x píxeles respecto a 1080p) o limitar
a **30 FPS** (`Limite de FPS`), que con el limiter es estable y no cae a tirones.

## 8. Por qué el FPS saltaba 60 ↔ 30 (y el fix)

Síntoma reportado: en combate el juego pasa de 60 a 30 y vuelve, sin valores
intermedios. Los logs (`[sh-swap]`) lo confirman: la serie es **bimodal**
(60/61 y 30/31, con 1 s suelto de 38–55 en las transiciones).

- **No era doble buffer**: el swapchain es `DXGI_SWAP_EFFECT_FLIP_DISCARD` con
  **3 buffers** (`D3D12Presenter::PaintContext::kSwapChainBufferCount`) y el
  command processor permite **3 frames en cola** (`kQueueFrames = 3`).
- **Causa**: el `Present(0, …)` **sin** `ALLOW_TEARING` se sincroniza con el
  vblank. Un frame que tarda 17 ms espera al siguiente vblank (33 ms), y como el
  siguiente vuelve a pasarse, el juego se queda **clavado en 30** (mitad de
  vblank) hasta que la carga baja. El limiter de 60 no lo evita: solo reparte
  los presents dentro de esa rejilla.
- **Fix**: **tearing adaptativo** (`d3d12_present_adaptive_tearing=true` por
  defecto). El swapchain se crea siempre con `ALLOW_TEARING` (si DXGI lo
  soporta) y el presenter mide lo que tarda el frame en estar listo (desde que
  volvió el present anterior hasta antes de esperar al limiter, con suavizado y
  histeresis): si está por encima de **0.95 × 16.6 ms** pasa
  `DXGI_PRESENT_ALLOW_TEARING` (no bloquea → 58, 52, 45… progresivo); si hay
  margen (<0.80 ×) lo quita y el present va sincronizado al vblank.
  - **A 60 FPS no hay desgarro** (el present vuelve a estar sincronizado). Antes
    de este ajuste se pasaba `ALLOW_TEARING` en *todos* los frames, y como el
    limiter de 60 deja los presents justo después del vblank, se veía una línea
    de desgarro en la **parte superior** de la pantalla.
  - Por debajo de 60 sí puede haber algo de desgarro puntual: es el precio de no
    clavarse en 30. Con **VRR/G-Sync** (`d3d12_allow_variable_refresh_rate_and_tearing=true`,
    "siempre (VRR)") no se ve ninguno.
  - Para el comportamiento antiguo (60/30 sin desgarro): las dos cvars a `false`
    (F5 → **Tearing / VRR** → *desactivado (60/30)*).

## Referencias

- Xenia wiki, *ROV*: por qué existe la ruta y sus límites.
- Xenia Canary issue #161 (PGR4): "with RTV any light sources don't seem to emit
  light… ROV fixes it" — misma clase de bug que Splatterhouse.
- `extern/rexglue-sdk/src/graphics/d3d12/render_target_cache.cpp` (selección de ruta).
- `extern/rexglue-sdk/src/graphics/pipeline/shader/dxbc_translator_om.cpp` (OM ROV).
- `extern/rexglue-sdk/src/graphics/vulkan/render_target_cache.cpp` (fbo vs fsi).
