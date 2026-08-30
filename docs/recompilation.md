# Recompilación — Splatterhouse Xbox 360

## Flujo XenonRecomp

```
default.xex (encriptado XEX2)
   |  xextool -r -c
   v
default.dec.xex (PE PPC desencriptado)
   |  XenonRecomp (ppc -> C++)
   v
recompiled/*.cpp + *.h  (funciones PPC como C++ con contexto)
   |  + src/os HLE + XenonUtils runtime
   v
splatterhouse.exe (x64 nativo)
```

## Tipos de código

- **Recompilado puro**: lógica del juego (update, AI, física). Va a `recompiled/`.
- **HLE**: llamadas al OS/GPU/Audio que el juego hace via imports. Se reimplementan en `src/os/` y `src/gpu|audio|input`.

## Cómo averiguar qué implementar primero

1. Recompila y compila. Ejecuta con `logLevel=0` (trace).
2. Mira los primeros `Unimplemented: xam!... / xboxkrnl!... / d3d9!...` en log.
3. Implementa esos stubs (ver `src/os/hle_*.cpp:RegisterAll()`).
4. Repite.

Herramientas útiles:
- `xenia` (referencia de implementación HLE Lee `xenia/kernel/xboxkrnl*`)
- IDA/Ghidra con PPC plugin para ver strings y xrefs de `MaxSmoothedFrameRate`, `ResX`, etc.
- `tools/recompile.py --dry-run` para validar sin binario.

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
