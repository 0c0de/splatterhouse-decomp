#!/usr/bin/env python3
"""
Pipeline de recompilación para Splatterhouse (Xbox 360) con XenonRecomp.
Uso:
  python tools/recompile.py --xex game/default.dec.xex
  python tools/recompile.py --xex game/default.xex --decrypt  # intenta desencriptar con xextool si está instalado
  python tools/recompile.py --dry-run  # solo valida config
"""
import argparse, subprocess, shutil, sys, os
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
RECOMP_TOML = ROOT / "config" / "recomp.toml"
XENON_RECOMP_BIN = ROOT / "extern" / "XenonRecomp" / "build" / "XenonRecomp"
PPC_CONTEXT_H = ROOT / "extern" / "XenonRecomp" / "XenonUtils" / "ppc_context.h"

def find_xenonrecomp():
    candidates = [
        XENON_RECOMP_BIN,
        XENON_RECOMP_BIN.with_suffix(".exe"),
        ROOT / "build" / "windows-vs2026-release" / "extern" / "XenonRecomp" / "Release" / "XenonRecomp.exe",
        ROOT / "build" / "windows-vs2026-debug" / "extern" / "XenonRecomp" / "Debug" / "XenonRecomp.exe",
        ROOT / "build" / "windows-vs2026-release" / "extern" / "XenonRecomp" / "XenonRecomp.exe",
        ROOT / "build" / "windows-release" / "extern" / "XenonRecomp" / "XenonRecomp.exe",
        ROOT / "build" / "windows-vs-release" / "extern" / "XenonRecomp" / "Release" / "XenonRecomp.exe",
        Path(shutil.which("XenonRecomp") or ""),
        Path(shutil.which("XenonRecomp.exe") or ""),
    ]
    for c in candidates:
        if c and c != Path("") and Path(c).is_file():
            return Path(c)
    # Fallback: búsqueda recursiva en build/
    for p in (ROOT / "build").rglob("XenonRecomp.exe"):
        if p.is_file():
            return p
    return None

def main():
    ap = argparse.ArgumentParser(description="Recompila Splatterhouse XEX con XenonRecomp")
    ap.add_argument("--xex", type=Path, help="Ruta al .xex (desencriptado idealmente)")
    ap.add_argument("--config", type=Path, default=RECOMP_TOML, help="Ruta a recomp.toml")
    ap.add_argument("--ppc-context", type=Path, default=PPC_CONTEXT_H, help="Ruta a ppc_context.h (XenonUtils)")
    ap.add_argument("--decrypt", action="store_true", help="Intenta desencriptar con xextool")
    ap.add_argument("--dry-run", action="store_true")
    ap.add_argument("--xenonrecomp", type=Path, help="Binario XenonRecomp manual")
    args = ap.parse_args()

    print(f"[recompile] Root: {ROOT}")
    if not args.config.exists():
        print(f"[recompile] No existe {args.config}, usando config/recomp.toml.example como plantilla")
        ex = ROOT / "config" / "recomp.toml.example"
        if ex.exists():
            # crear config temporal con input sobreescrito
            import shutil
            ROOT.joinpath("config").mkdir(exist_ok=True)
            shutil.copy(ex, args.config)
            print(f"  -> creado {args.config}")

    if args.xex and not args.xex.exists():
        print(f"[error] XEX no encontrado: {args.xex}", file=sys.stderr)
        sys.exit(1)

    xr = Path(args.xenonrecomp) if args.xenonrecomp else find_xenonrecomp()
    if not xr or not xr.is_file():
        print(f"[warn] XenonRecomp no encontrado. Compilalo primero:", file=sys.stderr)
        print(f"  git submodule update --init --recursive", file=sys.stderr)
        print(f"  cmake --preset windows-vs2026-release && cmake --build --preset windows-vs2026-release --target XenonRecomp", file=sys.stderr)
        print(f"  Buscado en: build/**/XenonRecomp.exe y extern/XenonRecomp/build/", file=sys.stderr)
        if xr and Path(xr).exists() and not Path(xr).is_file():
            print(f"  [hint] Encontrado directorio pero no exe: {xr} -> verifica que hayas compilado", file=sys.stderr)
        if not args.dry_run:
            sys.exit(1)
    else:
        print(f"[recompile] XenonRecomp: {xr}")
    print(f"[recompile] PPC context: {args.ppc_context} (exists={args.ppc_context.exists()})")
    if not args.ppc_context.exists():
        print(f"[error] ppc_context.h no encontrado: {args.ppc_context}", file=sys.stderr)
        print(f"  Esperado en: {PPC_CONTEXT_H}", file=sys.stderr)
        sys.exit(1)

    if args.decrypt and args.xex:
        xextool = shutil.which("xextool")
        if not xextool:
            print("[warn] xextool no en PATH, saltando desencriptado")
        else:
            dec = args.xex.with_suffix(".dec.xex")
            print(f"[decrypt] {args.xex} -> {dec}")
            subprocess.run([xextool, "-r", "-c", str(args.xex)], check=False)

    if args.dry_run:
        print("[dry-run] OK - config y binario validados")
        return

    # XenonRecomp requiere 2 args: [input TOML] [PPC context header]
    # Ver README: XenonRecomp [input TOML file path] [PPC context header file path]
    cmd = [str(xr), str(args.config), str(args.ppc_context)] if xr else []
    print(f"[recompile] Ejecutando: {' '.join(cmd)}")
    # Asegurar que out_directory_path existe
    try:
        import tomllib as _toml
    except ImportError:
        try:
            import tomli as _toml
        except ImportError:
            _toml = None
    if _toml and args.config.exists():
        try:
            import pathlib as _pl
            txt = args.config.read_text(encoding="utf-8")
            # parse simple para out_directory_path
            for line in txt.splitlines():
                if "out_directory_path" in line and "=" in line:
                    out = line.split("=",1)[1].strip().strip('"').strip("'")
                    out_path = (args.config.parent / out).resolve() if not Path(out).is_absolute() else Path(out)
                    out_path.mkdir(parents=True, exist_ok=True)
                    print(f"[recompile] out_directory: {out_path} (creado si no existia)")
        except Exception as e:
            print(f"[warn] no se pudo asegurar out_directory: {e}")
    ret = subprocess.run(cmd)
    sys.exit(ret.returncode)

if __name__ == "__main__":
    main()
