#!/usr/bin/env python3
"""
Codegen para Splatterhouse (Xbox 360) con el CLI de ReXGlue.

Uso:
  python tools/recompile.py                                 # codegen con el manifest
  python tools/recompile.py --xex game/default.dec.xex      # valida que exista el XEX
  python tools/recompile.py --force                         # ignora errores de validacion
  python tools/recompile.py --cli C:\\ruta\\a\\rexglue.exe  # binario manual

El CLI de ReXGlue se busca en este orden:
  1. --cli / %REXGLUE_BIN%
  2. rexglue en PATH
  3. Build local del SDK: build/rexglue-sdk/bin/recxglue(.exe), SDK instalado con `--target install`

Documentacion: https://github.com/rexglue/rexglue-sdk/wiki
"""
from __future__ import annotations

import argparse
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
MANIFEST = ROOT / "splatterhouse_manifest.toml"


def find_rexglue(cli: str | None) -> Path | None:
    if cli:
        p = Path(cli)
        if not p.is_file():
            sys.exit(f"[error] CLI no encontrado: {p}")
        return p
    env = shutil.os.environ.get("REXGLUE_BIN")
    if env and Path(env).is_file():
        return Path(env)
    for name in ("rexglue", "rexglue.exe"):
        if (which := shutil.which(name)):
            return Path(which)
    for base in (ROOT / "extern" / "rexglue-sdk" / "out", ROOT / "build",
                 ROOT / "out" / "build"):
        for p in base.rglob("rexglue.exe"):
            if p.is_file():
                return p
        for p in base.rglob("rexglue"):
            if p.is_file():
                return p
    return None


def main() -> int:
    ap = argparse.ArgumentParser(description="Codegen de Splatterhouse con ReXGlue")
    ap.add_argument("--xex", type=Path, default=(ROOT / "game" / "default.dec.xex"),
                    help="XEX desencriptado (validacion; el manifest apunta a game/default.dec.xex)")
    ap.add_argument("--cli", type=str, help="Ruta al binario rexglue")
    ap.add_argument("--force", action="store_true",
                    help="Genera salida aunque haya errores de validacion (UnresolvedCall etc.)")
    ap.add_argument("--dry-run", action="store_true", help="Solo valida config y XEX")
    args = ap.parse_args()

    if not MANIFEST.exists():
        sys.exit(f"[error] No existe {MANIFEST}")
    if not args.xex.exists():
        print(f"[error] XEX no encontrado: {args.xex}", file=sys.stderr)
        print("  Extrae el ISO con exiso/xiso y desencripta:", file=sys.stderr)
        print("  xextool -r -c default.xex -> game/default.dec.xex", file=sys.stderr)
        return 1
    print(f"[recompile].Manifest: {MANIFEST}")
    print(f"[recompile] XEX:      {args.xex}")

    if args.dry_run:
        print("[recompile] Modo --dry-run: no se ejecuta codegen.")
        return 0

    rexglue = find_rexglue(args.cli)
    if not rexglue:
        print("[error] CLI de ReXGlue no encontrado.", file=sys.stderr)
        print("  1. Descarga el release: https://github.com/rexglue/rexglue-sdk/releases", file=sys.stderr)
        print("  2. O compila el SDK y: cmake --build out/build/win-amd64 --target install", file=sys.stderr)
        print("  3. Tambien puedes definir REXGLUE_BIN o usar --cli", file=sys.stderr)
        return 1
    print(f"[recompile] rexglue:   {rexglue}")

    cmd = [str(rexglue), "codegen", str(MANIFEST)]
    if args.force:
        cmd.append("--force")
    print(f"[recompile] {' '.join(cmd)} ")
    result = subprocess.run(cmd, cwd=ROOT)
    if result.returncode != 0:
        print("[error] codegen fallo ( Usa --force para generar igualmente y "
              "resolver los errores incrementalmente", file=sys.stderr)
        return result.returncode
    print("[recompile] Generado en generated/. Compilar: cmake --build out/build/win-amd64 -j")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
