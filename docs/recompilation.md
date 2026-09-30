# Recompilación — Splatterhouse Xbox 360

## Flujo ReXGlue

```
default.xex (encriptado XEX2)
   |  xextool -r -c
   v
default.dec.xex (IMAGE PPC desencriptado)
   |  rexglue codegen splatterhouse_manifest.toml
   v
generated/*.cpp + _init.h/.cpp (funciones PPC como C++ con PPCContext)
   |  + Runtime del SDK (rex::Runtime, KernelState, FunctionDispatcher)
   v
splatterhouse.exe (x64 nativo, Clang)
```

## Tipos de código

- **Recompilado puro**: lógica del juego (update, AI, física). Va a `generated/` (autogenerado, no editar).
- **Overrides/HLE**: llamadas al kernel se reimplementan como `REX_EXPORT`/`REX_HOOK` (ver wiki Function Overrides). El SDK ya trae shims de xboxkrnl/xam; añade los que falten en `src/`.
- **Mid-ASM hooks**: parcheo fino por instrucción vía `[[midasm_hook]]` en `config/codegen.toml`.

## Cómo averiguar qué implementar primero

1. `rexglue codegen splatterhouse_manifest.toml --force` y compila. La primera vez habrá `UnresolvedCall`: lista `config/codegen.toml` con `[functions]`.
2. Ejecuta y mira los primeros logs del runtime (activa `--log_level trace` / cvar `log_level`).
3. Implementa los imports que falten con `REX_EXPORT` o stubs `REX_EXPORT_STUB`.
4. Repite.

Herramientas útiles:
- `xenia` (referencia de implementación; ReXGlue deriva de su runtime)
- IDA/Ghidra con PPC plugin para ver strings y xrefs de `MaxSmoothedFrameRate`, `ResX`, etc.
- `python tools/recompile.py --dry-run` para validar rutas sin binario.

## UE3 particularidades (Splatterhouse)

Splatterhouse 2010 usa Unreal Engine 3 (~2009). Pistas:
- Binario contiene strings `Unreal`, `GWorld`, `GEngine`, `GFx`, `Bink`
- Configs en `Coalesced.ini` / `*.ini` dentro de `game/CookedXenon/`
- Para unlock FPS busca `bSmoothFrameRate`, `Min/MaxSmoothedFrameRate` en memoria o .ini.
- Scaleform GFx para HUD/menus -> puede necesitar HLE de `GFx` o simplemente dejar que el recompilado lo llame (es parte del juego, no import OS).

## Patches

Ver `config/patches.toml.example`. Dos tipos:
- **Bin patch en recompilación**: XenonRecomp puede noppear branches.
- **Runtime patch**: Parchear memoria guest tras `mem::Initialize()` (ej. escribir `60 00 00 00` en addr).

## Debugging

- `config.toml` -> `logLevel=0` + `devMode=true`
- En MSVC: `F5` con preset `windows-debug` deja consola abierta con logs.
- Para trace PPC: habilitar `XenonUtils` trace (si se integra).
