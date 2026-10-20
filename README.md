# Splatterhouse Recompiled — native Xbox 360 → PC port

Native PC port of **Splatterhouse (2010)** (Xbox 360 version, title id `4E4D07F0`), built by
**static recompilation** with the [ReXGlue SDK](https://github.com/rexglue/rexglue-sdk). The game
code is translated to native C++ and compiled for your PC: **it is not an emulator**, there is no
runtime interpretation, and it runs at native speed (60 FPS).

> **It does not contain any game code or assets.** You need your own copy of the game (disc/ISO).
> Inspired by *Sonic Unleashed Recompiled*, *Ninja Gaiden 2 Black Recompiled* and
> *Silent Hill: Downpour Recompiled*.

**Status: playable end-to-end** — menus, full campaign, DLC, 60 FPS, localized text and ultrawide.

---

## Quick start (what you need to do)

**Requirements**

- Windows 10/11 **x64**.
- A GPU with **Direct3D 12 + Rasterizer-Ordered Views** (NVIDIA GTX 900+, AMD RX 400+, Intel Arc).
- Your own **Splatterhouse (Xbox 360) ISO**.

**Steps**

1. **Download the release** and extract it anywhere (not inside `Program Files`). You get
   `splatterhouse.exe`, the DLLs, `install.bat`, `tools\` and optionally `shadercache\`.
2. **Drag your Splatterhouse ISO onto `install.bat`** (or run `install.bat` and paste the path).
3. **Run `splatterhouse.exe`** and play. The first boot takes a little longer while the shader cache
   is loaded; afterwards it starts in a couple of seconds.

That's it. The installer creates `splatterhouse.toml` for you with sane defaults (see
[Configuration](#configuration-splatterhousetoml-next-to-the-exe)).

**What the installer does**

- Extracts the ISO with `tools\extract-xiso.exe` into **`assets\`** (`-s` skips `$SystemUpdate`).
- Decrypts the XEX with `tools\xextool.exe` to `assets\default.dec.xex` (the runtime also accepts the
  retail `default.xex`; the decrypted one is used if present).
- Creates `splatterhouse.toml` if it does not exist, choosing the **internal resolution based on your
  GPU**: **AMD/Intel → 1280x720**, NVIDIA → 1920x1080 (both are upscaled to your window with FSR).
  You can change it any time with **F5 → Preset**.
- Copies `shadercache\*` (if the release ships it) to `%USERPROFILE%\Documents\splatterhouse\cache\shaders\shareable`,
  which avoids most first-time shader stutters.
- Verifies that the extraction produced `default.xex` + `data\`, and warns if not.

**Folder layout after installing**

```
splatterhouse.exe
splatterhouse.toml          # created by install.bat
rexruntimerd.dll            # runtime
rexgpu-xenosrd.dll          # GPU (Xenos/D3D12) plugin
amd_fidelityfx_dx12drel.dll # FSR/CAS (only in builds with FidelityFX)
assets\
  default.xex               # retail XEX (from the ISO)
  default.dec.xex           # decrypted XEX (installer)
  data\...                  # game data
dlc\                        # optional: drop DLC packages here (created on startup)
```

---

## Controls

| Key | Action |
|---|---|
| **F3** | Debug overlay (FPS, frame stats) |
| **F4** | Runtime settings overlay (every cvar) |
| **F5** | **Port options screen** (graphics, language, 60 FPS patch, level unlock…) |
| **F7** | Achievements |
| **`** (backtick) | Console |

The game itself is played with a **gamepad** (Xbox/XInput-compatible; the input backend is
configurable with `input_backend = "sdl"` or `"xinput"`).

The options screen (`F5`) is the port's own, in the game's visual style. **The labels are in Spanish
at the moment** (`Resolucion interna` = internal resolution, `Aspecto interno` = internal aspect,
`Pantalla completa` = fullscreen, `Idioma` = language, `Salir del juego` = quit).

**Using the F5 screen**

- ↑/↓ (or the mouse wheel) move the selection, ←/→ change the value, **S** saves to
  `splatterhouse.toml`, **Esc** or **F5** closes. Mouse click also works.
- Rows tagged `[reinicio]` need a restart to apply (video mode).
- Saving rewrites the file; the previous version is kept as `splatterhouse.toml.bak`.
- **Presets** change only the *vertical* internal resolution (Low 540p / Medium 720p / High 900p /
  Ultra 1080p) and keep your aspect ratio, so they don't break an ultrawide setup.

### Ultrawide (21:9, 32:9)

Ultrawide works **automatically**: on startup the *width* of the internal video mode is matched to
the aspect ratio of your window/monitor (`sh_aspect = "auto"`), so the game renders that aspect
natively — no pillarboxing, no stretching. The log shows it:

```
[sh-aspect] auto: modo interno 2560x1080 (antes 1920x1080, pantalla 2.37:1)
```

You can override it in `F5`:

- **Aspecto interno** → `auto` (default), `16:9`, `21:9`, `32:9`.
- **Resolucion interna** → up to `3440x1440` (including `2560x1080`); picking one sets `sh_aspect` to
  `keep` so your choice is respected.

Note that a wider mode costs more pixels in the ROV path (~33 % more from 16:9 1080p to 21:9 1080p);
if combat performance suffers, lower the preset.

### Frame rate: 60 ↔ 30 jumping and tearing

The game is synchronized to the guest vblank (60 Hz). If the monitor presentation is also
vblank-synchronized, any frame that takes **more than 16.6 ms** waits for the *next* vblank (33 ms)
and, since the next frame is late again, the game gets **stuck at 30** until the load drops. It is not
a double-buffer problem: the port uses a 3-buffer flip chain (DXGI `FLIP_DISCARD`, 3 buffers, up to 3
frames queued) and a 3-image guest output mailbox.

The presenter uses **adaptive tearing** (`d3d12_present_adaptive_tearing = true`): it measures the
real guest frame rate and allows tearing **only on frames that miss the 60 FPS budget**. Result:

- **At 60 FPS** the present is vblank-synchronized → **no tearing**.
- **Below 60 FPS** the present stops blocking → the frame rate degrades progressively (58, 52, 45…)
  instead of locking to 30. There may be slight tearing while it is below 60: that's the price of not
  dropping to 30.

`F5 → Tearing / VRR` has three states: `auto (solo si baja de 60)` (default), `siempre (VRR)` (always
tear — ideal with **G-Sync/FreeSync**) and `desactivado (60/30)` (old behaviour, never tear).

---

## Features

**Working**

- **Full static recompilation** of the game (~208 generated C++ files); no known crashes in gameplay.
- **Correct 3D lighting and post-processing** through the D3D12 **ROV** path (reproduces the Xbox 360
  eDRAM; the alternative RTV path loses the lighting).
- **60 FPS** with the official 60 FPS patch applied durably (data + one instruction).
- **FSR** (FidelityFX EASU/RCAS) upscaling, plus CAS and FXAA options.
- **Ultrawide** (21:9/32:9) rendered natively, automatic.
- **Localized text** (Spanish/French/Italian/German/Japanese…), audio always in English.
- **DLC support** (Survival Arenas, masks…): drop the packages in `dlc\` and they are installed
  automatically.
- **All story chapters selectable** (level unlock, on by default).
- **Installer**: drag the ISO and you're done.
- **Precompiled shader cache** support to avoid stutters.

**Known limitations**

- **Stutters the first time a new area is loaded** if the shader cache is incomplete: the port has to
  translate the shader and create the D3D12 pipeline (~2–11 ms each, hundreds in a row). The SDK does
  it on background threads and skips the draw until it's ready (brief popping, no freeze), and
  pre-creates the whole cache at startup. **Shipping a complete `shadercache\`** is the fix
  (`tools\pack_shadercache.bat`).
- **ROV performance on AMD/Intel**: the D3D12 interlock is much more expensive in **combat** (many
  enemies + shadow passes), which is why the installer defaults those GPUs to 720p. See
  `docs/rov-performance.md` §7.
- **`rtv` render path is not usable**: the scene goes almost black. Always use `rov` (the default).
- **The options live in a separate screen (F5)**, not inside the game's own menu: the UI is Scaleform
  GFx compiled in `.gfx` files (with Xbox 360 XCompress compression and no `.fla` sources), so it can't
  be extended.
- **Windows x64 only** (D3D12).

---

## DLC (Survival Arenas, masks, etc.)

Drop the **STFS packages** (files whose first 4 bytes are `CON `, `LIVE` or `PIRS`, often without
extension or with `.xcp`) into a folder named **`dlc`** next to `splatterhouse.exe`. The port creates
that folder on startup, logs its path, and installs the packages automatically (extraction + header +
license mask); they then appear in the game menu.

```
<game folder>\dlc\35A606675772B2C90230F20D17FA0F11E55C1B594E
<game folder>\dlc\9E079D75E93C05D42B269A99751474F659AACE954E
```

**Other accepted locations** (the port scans all of them):

- `%USERPROFILE%\Documents\splatterhouse\4E4D07F0\00000002\`
- `%USERPROFILE%\Documents\splatterhouse\0000000000000000\4E4D07F0\00000002\`
- `%USERPROFILE%\Documents\splatterhouse\<profile-xuid>\4E4D07F0\00000002\` (e.g. `B13EBABEBABEBABE`)

**Troubleshooting**

- Nothing appears? Launch with the environment variable **`REX_SH_DLC=1`** (`set REX_SH_DLC=1` in a
  command prompt, then run the exe) to log exactly what content the game enumerates/opens, and check
  `splatterhouse.log`.
- Make sure the files really are STFS packages (hex-view the first 4 bytes: `CON `/`LIVE`/`PIRS`).
- Installation is idempotent: after the first run the content is extracted to
  `...\0000000000000000\4E4D07F0\00000002\<name>\`, so you can delete the originals.

Full details (package format, load chain, diagnostics) in **`docs/dlc.md`**.

---

## Troubleshooting

**The game closes by itself right after starting (black screen).**
The game data isn't where the runtime looks for it (usually the ISO was extracted by hand without the
`data` folder, or into a subfolder). The port detects this *before* booting and exits with a readable
message:

- The details are also written to **`assets_error.txt`** next to the exe (which folder it checked and
  what is missing).
- It accepts the data in **`assets\`**, in the **exe folder itself**, or in a **single subfolder of
  `assets\`** (e.g. `assets\Splatterhouse\default.xex`).
- Point it somewhere else with `game_data_root = "D:\path"` in `splatterhouse.toml` (or
  `--game_data_root=...`).
- Escape hatch: `sh_skip_assets_check = true`.

**Black screen but the game is running.**
`splatterhouse.toml` is missing or has no `gpu_plugin`; the default is `gpu_plugin = "xenos"` and
`render_target_path_d3d12 = "rov"`. Run `install.bat` or create the file (see
[Configuration](#configuration-splatterhousetoml-next-to-the-exe)).

**Combat is slow (especially AMD/Intel GPUs).**
Lower the internal resolution: **F5 → Preset → Low/Medium**, or `F5 → Resolucion interna`. The ROV
cost scales with pixels; 720p has ~2.2× fewer pixels than 1080p. See `docs/rov-performance.md` §7.

**I see a horizontal tear line.**
It should normally not happen at 60 FPS (adaptive tearing). If you are below 60 FPS it can appear; set
`F5 → Tearing / VRR → desactivado (60/30)` to never tear (you'll get the 60/30 behaviour instead), or
enable G-Sync/FreeSync in your GPU panel and use `siempre (VRR)`.

**Where are the logs?**
`splatterhouse.log` next to the exe (`log_level = "warn"` by default; raise it to `info` or `trace` for
more detail). Archived logs are in `logs\`.

---

## Configuration (`splatterhouse.toml`, next to the exe)

| cvar | values | notes |
|---|---|---|
| `gpu_plugin` | `xenos` | **required** (empty = no GPU = black screen) |
| `render_target_path_d3d12` | `rov` | **required** (`rtv` breaks the lighting) |
| `present_effect` | `fsr` (or `bilinear`, `cas`, `fsr2`, `fsr3`) | presentation upscaling (FidelityFX builds) |
| `video_mode_width/height` | e.g. `1920/1080` | guest internal resolution (lower it to gain FPS) |
| `sh_aspect` | `auto`/`16:9`/`21:9`/`32:9`/`keep` | internal aspect; `auto` matches the window (ultrawide native) |
| `video_mode_refresh_rate` | `60` | guest video mode refresh rate |
| `vsync` | `false` | guest vblank is 60 Hz regardless |
| `d3d12_present_frame_limiter[_fps]` | `true` / `60` | host present rate cap |
| `d3d12_present_adaptive_tearing` | `true` | tear only when the frame misses 60 FPS |
| `d3d12_allow_variable_refresh_rate_and_tearing` | `false` | `true` = always tear ("VRR mode") |
| `sh_language` | `auto`/`english`/`spanish`/… | **text** language (audio is always English) |
| `sh_60fps` | `true` | 60 FPS patch |
| `sh_unlock_levels` / `sh_unlock_levels_max` | `true` / `7` | chapter select unlock |
| `log_level` | `warn` / `info` / `trace` | log verbosity |

---

# Development

## How this port was made

There is no source code available: the port starts from the **retail XEX** and is **statically
recompiled**.

1. **ISO extraction** (`tools/extract-xiso.exe -x -s`): produces `default.xex` + `data/`.
2. **XEX decryption** (`tools/xextool.exe -e u -c b`): produces `default.dec.xex` (codegen input).
3. **Codegen** (`rexglue codegen splatterhouse_manifest.toml`): ReXGlue analyses the PowerPC code,
   discovers functions and emits **recompiled C++** (`generated/*.cpp`, ~208 files) plus a function
   registry and the guest image.
4. **Runtime** (SDK): 4 GB guest memory, xboxkrnl/xam kernel shims, VFS/STFS (XContent), threading,
   GPU (Xenos → D3D12), audio (XMA/SDL), input (SDL/XInput).
5. **SDK patches** (see `AGENTS.md`): codegen fixes (non-ABI register propagation `r0/r2/r11/r12` —
   fixes `__finally`/`__chkstk`), function discovery via data pointers, `vmaxuw`, access-violation
   instrumentation, adaptive present pacing, content installation, etc.
6. **Game patches**, applied durably in `src/` (not in `generated/`, which is gitignored and
   regenerated):
   - **60 FPS**: data (`0x82F8FF58/5C`: 1/15 and 1/30 → **1/60**) plus one instruction (`0x8247A4AC`)
     replicated with a **strong override** of the codegen's weak function.
   - **Localization**: override of `sub_827B65E0` that forces the language **only** on the
     language-load path (text), leaving audio in English.
   - **Level unlock**: overrides of `sub_82E5C540` / `sub_82EF0FE0` to report the max chapter.

The old in-house runtime (XenonRecomp) remains in `legacy/` (reference only).

## Building from source

**Requirements**

| Tool | Version | Notes |
|---|---|---|
| Windows 10/11 | — | D3D12 |
| Visual Studio 2022/2026 (Clang) or LLVM | Clang 18+ (tested 22.1.8) | |
| CMake | 3.25+ | |
| Ninja | 1.11+ | |
| Python | 3.9+ | `tools/` scripts |
| Git | 2.40+ | submodules |

**Clone and set up**

```powershell
git clone https://github.com/0c0de/splatterhouse-decomp.git
cd splatterhouse-decomp
git submodule update --init extern/rexglue-sdk
git -C extern/rexglue-sdk submodule update --init --recursive
```

**Put your copy of the game** in `game\` (gitignored):

```
game\default.xex        # retail, straight from the ISO
game\default.dec.xex    # decrypted with xextool -e u -c b (needed by codegen)
game\data\...           # the rest of the ISO
```

**Configure and build**

```powershell
# RelWithDebInfo = good performance (use this to play); Debug is very slow
cmake --preset win-amd64-relwithdebinfo
#    optional FSR/FidelityFX:
#    cmake --preset win-amd64-relwithdebinfo -DREXGLUE_ENABLE_FIDELITYFX=ON

cmake --build out\build\win-amd64-relwithdebinfo --target splatterhouse -j 6
```

Codegen runs automatically when its stamp is stale. To regenerate manually (~800 s, 208 files):

```powershell
extern\rexglue-sdk\out\win-amd64\rexglued.exe codegen splatterhouse_manifest.toml --ignore-stamp
```

**Run**

The executable loads the DLLs **from its own directory**, so always run it from the build folder:

```powershell
cd out\build\win-amd64-relwithdebinfo
.\splatterhouse.exe        # log -> splatterhouse.log
```

The build folder needs the game data. Either copy the ISO contents there or create an `assets`
junction to the repo's `game` folder (the port looks for `assets\`, or the exe folder itself):

```powershell
New-Item -ItemType Junction -Path out\build\win-amd64-relwithdebinfo\assets -Target "$PWD\game"
```

**Important notes**

- After rebuilding the runtime (`--target rexruntime`), copy the DLLs from
  `extern\rexglue-sdk\out\win-amd64\` (`rexruntimerd.dll`, `rexgpu-xenosrd.dll`, and their `.pdb`) into
  your build folder — the SDK emits them to its own output directory, and the exe will happily load an
  old copy otherwise.
- With **FidelityFX** (`-DREXGLUE_ENABLE_FIDELITYFX=ON`) an extra **`amd_fidelityfx_dx12drel.dll`** is
  produced; it must sit next to the exe **and** next to `rexruntimed.dll`/`rexglued.exe`, or codegen
  fails with `0xC0000135` (DLL not found).
- The build preset `win-amd64-debug` is for debugging (unoptimized recompiled code: expect low FPS).

**Making a release folder**: `tools\make_release.bat` copies the exe, the DLLs, `install.bat`, `tools\`,
the docs and (if present) `shadercache\` into `splatterhouse-decomp-release\`, ready to zip. It
deliberately does **not** ship a `splatterhouse.toml` so that `install.bat` can generate it on the
user's machine with the right internal resolution for their GPU.

## Project layout

```
install.bat                 # installer (drag the ISO onto it)
assets/                     # installed game data (gitignored)
game/                       # your copy of the game for development (gitignored)
tools/
  extract-xiso.exe          # extract the Xbox 360 ISO
  xextool.exe               # decrypt the XEX
  pack_shadercache.bat      # pack the shader cache for the release
  make_release.bat          # build "splatterhouse-decomp-release\" with everything needed
  setup.ps1 / recompile.py  # setup and codegen (dev utilities)
generated/                  # recompiled code (autogenerated; gitignored)
config/codegen.toml         # codegen flags + [functions] overrides
src/
  main.cpp                  # REX_DEFINE_APP + port cvars
  splatterhouse_app.h       # ReXApp: asset paths, patches, DLC install, aspect, defaults
  sh_60fps_hook.cpp         # 60 FPS patch (instruction)
  sh_language_hook.cpp      # text localization
  sh_unlock_hook.cpp        # level/chapter unlock
  sh_uf_diag.cpp            # defensive fix for a null virtual call
  graphics_menu.h           # F5 options screen
extern/rexglue-sdk/         # SDK submodule (with local patches)
docs/rov-performance.md     # ROV performance analysis (why ROV, how to speed it up)
docs/dlc.md                 # DLC support and installation
AGENTS.md                   # technical log (status, SDK patches, decisions)
legacy/                     # old XenonRecomp runtime (reference only)
```

---

## Credits and legal notice

- **ReXGlue SDK** — recompilation runtime and toolchain.
- **Xenia** — the GPU/eDRAM, formats and many runtime foundations come from Xenia.
- **extract-xiso** (xboxdev) and **XexTool** (xorloser) — ISO/XEX tools.
- **60 FPS patch** — from `xenia-canary/game-patches` (author *illusion*), adapted to the recomp.
- **FidelityFX / FSR** — AMD.

This project is for **educational and preservation purposes**. It is not affiliated with Microsoft,
Bandai Namco or Konami, and **it does not include any game code or assets**: you need your own legal
copy to use it.
