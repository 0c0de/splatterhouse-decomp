# Splatterhouse Recompiled — Native Xbox 360 to PC port

Native PC port of **Splatterhouse (2010)** (Xbox 360 version, title id `4E4D07F0`) built by **static recompilation** with the [ReXGlue SDK](https://github.com/rexglue/rexglue-sdk). The game code is translated to native C++ compiled for your PC: **it is not an emulator**, there is no runtime interpretation, and it runs at native speed (60 FPS).

> Inspired by `Sonic Unleashed Recompiled`, `Ninja Gaiden 2 Black Recompiled` and `Silent Hill: Downpour Recompiled`.
> **It does not contain any game code or assets.** You need your own copy (disc/ISO).

---

## Status

**Playable.** It boots, the menus work, and the campaign is playable at **60 FPS** with correct lighting, localized text and English audio.

### Working
- **Full static recompilation**: 208 files, no known crashes in gameplay.
- **GPU (D3D12, ROV path)**: correct 3D scene (ROV reproduces the X360 eDRAM; RTV breaks the lighting).
- **60 FPS**: official 60 FPS patch applied durably (see below).
- **FSR (FidelityFX)**: upscaling/AA on the present.
- **Localization (text)**: the in-game menu can be Spanish/French/Italian/German/Japanese; audio is always English.
- **Installer**: drag your ISO and you're done.
- **Precompiled shader cache** to avoid stutters.

### Pending / known limitations
- **Stutters when shaders compile** the first time in new areas: the port translates the shader to D3D12 and creates the PSO (compiles the driver). The SDK already does this **on background threads** and **skips the draw** if not ready (brief popping, no freeze), and **precreates all the cache pipelines at startup** (measured: 401 PSOs in ~530 ms). The PSO itself is cheap (~2–11 ms) — the cost is "hundreds in a row". **Solution: ship a complete cache** (`shadercache/`, generated with `tools\pack_shadercache.bat`). Diagnosis: `sh_pso_log` cvar.
- **Lighting with RTV**: if you use the `rtv` path the scene goes almost black (design limitation of the RTV path; you **must use `rov`**). See `docs/rov-performance.md`.
- **ROV performance**: the long-term fix for modest GPUs is optimizing the ROV *pixel shader* (the SDK leaves it as a TODO: specialize it with static RT parameters). For now it is compensated with resolution **presets** + FSR.
- **In-game options menu**: a custom screen in the game's style (**F5** key), not literally integrated into the game's Scaleform menu (the `.gfx` can't be edited).
- **Post-processing/DoF** a bit excessive at the start (probably a 720p→window upscale).
- **Distribution**: Win x64 binaries only (D3D12).

---

## How this decompilation was made

There is no source code: it starts from the **retail XEX** and is **statically recompiled**.

1. **ISO extraction** (`tools/extract-xiso.exe`): produces `default.xex` + `data/`.
2. **XEX decryption** (`tools/xextool.exe -e u -c b`): produces `default.dec.xex` (only for codegen).
3. **Codegen** (`rexglue codegen splatterhouse_manifest.toml`): ReXGlue analyzes the XEX PPC, discovers functions and emits **recompiled C++** (`generated/*.cpp`, ~208 files) + a function registry and image.
4. **Runtime** (SDK): 4 GB guest memory, xboxkrnl/xam kernel shims, VFS/STFS, threading, GPU (D3D12), audio (XMA/SDL), input.
5. **Local SDK patches** (see `AGENTS.md`) to fix codegen bugs (non-ABI register propagation `r0/r2/r11/r12` → fixes `__finally`/`__chkstk`), discover functions via data pointers, implement `vmaxuw`, instrument access violations, etc.
6. **Game patches** applied durably in `src/` (not in `generated/`):
   - **60 FPS**: data (`0x82F8FF58/5C`: 1/15 and 1/30 → **1/60**) and one instruction (`0x8247A4AC`) replicated with a **strong override** of the codegen's weak function.
   - **Localization**: override of `sub_827B65E0` to force the language **only** on the language-load path (text), leaving audio in English.

The old in-house runtime (XenonRecomp) remains in `legacy/` (reference only).

---

## Installing for the end user (PC)

**Requirements**: Windows 10/11 x64, a GPU with **D3D12 + Rasterizer-Ordered Views** (modern NVIDIA/AMD), and your copy of Splatterhouse (Xbox 360 ISO).

1. Download and extract the release (the `.exe`, the DLLs, `install.bat` and `tools/`).
2. **Drag your Splatterhouse ISO onto `install.bat`**.
   - The script extracts the ISO (`extract-xiso`), moves it to `assets\`, decrypts the XEX with `xextool` (`assets\default.dec.xex`) and creates a default `splatterhouse.toml`.
   - If the release includes a `shadercache\` folder, it copies it to your shader cache (fewer stutters).
3. Run **`splatterhouse.exe`** and play.

Without `splatterhouse.toml` the GPU is not loaded (black screen); the installer creates one for you and `SplatterhouseApp::OnPreSetup` forces `gpu_plugin="xenos"` as a safety net.

---

## Building from source (developers)

### Requirements
| Tool | Version | Notes |
|---|---|---|
| Windows 10/11 | — | D3D12 |
| VS 2022/2026 (Clang) or LLVM | Clang 18+ (tested 22.1.8) | |
| CMake | 3.25+ | |
| Ninja | 1.11+ | |
| Python | 3.9+ | `tools/` scripts |
| Git | 2.40+ | submodules |

### Steps

```powershell
# 0) SDK submodule
git submodule update --init extern/rexglue-sdk
git -C extern/rexglue-sdk submodule update --init --recursive

# 1) Configure (relwithdebinfo = good performance; debug = slow)
cmake --preset win-amd64-relwithdebinfo
#    (optional FSR)  -DREXGLUE_ENABLE_FIDELITYFX=ON

# 2) Put your copy of the game (gitignored)
#    game/default.xex     (retail, straight from the ISO)
#    game/default.dec.xex (decrypted with xextool -e u -c b; for codegen)
#    game/data/...        (rest of the ISO)

# 3) Build (codegen runs automatically if the stamp is stale)
cmake --build out\build\win-amd64-relwithdebinfo --target splatterhouse -j 6
#    regenerate codegen manually (~800 s, 208 files):
#    extern\rexglue-sdk\out\win-amd64\rexglued.exe codegen splatterhouse_manifest.toml --ignore-stamp
```

The exe loads the DLLs from **its own directory**: after rebuilding `rexruntime` you must copy `extern\rexglue-sdk\out\win-amd64\rexruntimerd.dll` (and the other `*rd.dll`) into `out\build\win-amd64-relwithdebinfo\`.

With **FSR** (`-DREXGLUE_ENABLE_FIDELITYFX=ON`) an additional **`amd_fidelityfx_dx12drel.dll`** is produced (in `bin/`), which must sit next to the exe **and** next to `rexruntimerd.dll`/`rexglued.exe` or codegen fails with `0xC0000135`.

---

## Controls and options

| Key | Action |
|---|---|
| **F3** | Debug overlay (FPS, stats) |
| **F4** | Runtime settings overlay (all cvars) |
| **F5** | **Port OPTIONS screen** (language, graphics preset, FSR, VSync, FPS cap…) |
| **F7** | Achievements |
| **`** (backtick) | Console |

On the **F5** screen: arrows to navigate/change, **S** saves to `splatterhouse.toml`, Esc/F5 closes. Rows tagged `[reinicio]` require a restart.

Presets: **Low** 960x540 / **Medium** 1280x720 / **High** 1600x900 / **Ultra** 1920x1080 (they adjust internal resolution, FSR and texture cache).

---

## Configuration (`splatterhouse.toml`, next to the exe)

| cvar | values | notes |
|---|---|---|
| `gpu_plugin` | `xenos` | **required** (empty = no GPU = black screen) |
| `render_target_path_d3d12` | `rov` | **required** (`rtv` breaks the lighting) |
| `present_effect` | `fsr` (or `bilinear`, `cas`) | present upscaling |
| `video_mode_width/height` | e.g. `1920/1080` | guest internal resolution (lower it to gain FPS) |
| `video_mode_refresh_rate` | `60` | |
| `vsync` | `false` | |
| `d3d12_present_frame_limiter[_fps]` | `true` / `60` | FPS cap (avoids the drop to 30 caused by vsync) |
| `d3d12_allow_variable_refresh_rate_and_tearing` | `false` | no tearing (enable in your panel if you have VRR) |
| `sh_language` | `auto`/`spanish`/`english`/… | **text** language (audio is always English) |
| `sh_60fps` | `true` | 60 FPS patch |
| `log_level` | `warn` / `trace` | |

---

## DLC (Survival Arenas, masks, etc.)

The port **supports DLC**: just drop the **STFS packages** (magic `CON `/`LIVE`/`PIRS`)
into the user data folder and they are **installed automatically** at startup
(extraction + header + license mask). They then show up in the menu.

### Step-by-step (if you don't know how)

1. **Get the DLC packages.** These are the original Xbox 360 DLC files. They are
   files whose first 4 bytes are `CON `, `LIVE` or `PIRS` (often with no extension,
   or `.xcp`). They are **not** ISO files and you do **not** open them.
2. **Open your user data folder.** Press `Win + R`, paste the following and hit Enter:
   ```
   %USERPROFILE%\Documents\splatterhouse
   ```
   (If it does not exist yet, run the game once so it is created.)
3. **Create this folder structure** inside it (`4E4D07F0` is the title id):
   ```
   Documents\splatterhouse\4E4D07F0\00000002\
   ```
4. **Copy the DLC files** into that `00000002` folder. The result should look like:
   ```
   Documents\splatterhouse\4E4D07F0\00000002\35A606675772B2C90230F20D17FA0F11E55C1B594E
   Documents\splatterhouse\4E4D07F0\00000002\9E079D75E93C05D42B269A99751474F659AACE954E
   ...
   ```
   (The filenames are package ids — leave them as they are.)
5. **Start the game.** In the log you will see:
   ```
   Installed DLC package <hash>
   [sh-dlc] Instalados N paquete(s) DLC desde la carpeta de usuario
   ```
6. The DLC arenas/masks/items now appear in the menu.

**Alternative locations** (any of them works, the port scans all):
- `Documents\splatterhouse\4E4D07F0\00000002\`
- `Documents\splatterhouse\0000000000000000\4E4D07F0\00000002\`
- `Documents\splatterhouse\<profile-xuid>\4E4D07F0\00000002\` (e.g. `B13EBABEBABEBABE`)

**Troubleshooting**
- Nothing appears? Set the environment variable **`REX_SH_DLC=1`** before launching
  (`set REX_SH_DLC=1` in cmd, then run the exe) to log exactly what content the
  game enumerates/opens. Also check `splatterhouse.log`.
- Make sure the files really are STFS packages (check the first 4 bytes with a hex
  viewer: `CON `/`LIVE`/`PIRS`).
- Installation is **idempotent**: once installed, the files are extracted to
  `...\0000000000000000\4E4D07F0\00000002\<name>\`. You can delete the originals
  afterwards if you want.

Full details (format, load chain, diagnostics) in **`docs/dlc.md`**.

---

## Project structure

```
install.bat                 # installer (drag the ISO onto it)
assets/                     # installed game data (gitignored; junction to game/ in dev)
tools/
  extract-xiso.exe          # extract the Xbox 360 ISO
  xextool.exe               # decrypt the XEX
  pack_shadercache.bat      # pack the shader cache for the release
  make_release.bat          # create "splatterhouse-decomp-release\" with everything needed
  setup.ps1 / recompile.py  # setup and codegen (dev utilities)
generated/                  # recompiled code (autogenerated; gitignored)
config/codegen.toml         # codegen flags + [functions] overrides
src/
  main.cpp                  # REX_DEFINE_APP + cvars (sh_60fps, sh_language, sh_graphics_menu)
  splatterhouse_app.h       # ReXApp: patches, paths, menu, XEX, DLC install
  sh_60fps_hook.cpp         # 60 FPS patch (instruction)
  sh_language_hook.cpp      # text localization
  sh_unlock_hook.cpp        # level/chapter unlock
  sh_uf_diag.cpp            # defensive fix for null virtual call (chapter 6)
  graphics_menu.h           # OPTIONS screen (F5)
extern/rexglue-sdk/         # SDK (submodule, with local patches)
docs/rov-performance.md     # ROV performance analysis
docs/dlc.md                 # DLC support and installation
AGENTS.md                   # full technical log (status, patches, decisions)
legacy/                     # old XenonRecomp runtime (reference)
```

---

## Credits and legal notice

- **ReXGlue SDK** — recompilation runtime and toolchain.
- **Xenia** — the GPU/eDRAM, formats and many runtime foundations come from Xenia.
- **extract-xiso** (xboxdev) and **XexTool** (xorloser) — ISO/XEX tools.
- **60 FPS patch**: from `xenia-canary/game-patches` (author *illusion*), adapted to the recomp.
- **FidelityFX / FSR**: AMD.

This project is for **educational and preservation purposes**. It is not affiliated with Microsoft, Bandai Namco or Konami, and **it does not include any game code or assets**: you need your own legal copy to use it.
