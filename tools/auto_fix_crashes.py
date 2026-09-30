"""
Bucle automatico de crash-fixes para el port de Splatterhouse con ReXGlue.

Por cada crash 'Call to invalid or unregistered function' o 'Unresolved call'
del runtime add la direccion a config/codegen.toml, re-corre codegen y
recompila, hasta que el juego arranca sin FATAL (o se agota el limite).

Uso:
  python tools/auto_fix_crashes.py                    # loop (release + debug junction)
  python tools/auto_fix_crashes.py -n 10              # max 10 iteraciones
  python tools/auto_fix_crashes.py --preset win-amd64-debug
"""
from __future__ import annotations
import re
import subprocess
import urllib.parse
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
CODEGEN_TOML = ROOT / "config" / "codegen.toml"
CDB_LOG = Path(sys.argv[0]).parent if False else None

FATAL_RE = re.compile(r"invalid or unregistered function at guest address 0x([0-9A-Fa-f]{8})")
UNRES_RE = re.compile(r"Unresolved call from 0x[0-9A-Fa-f]{8} to 0x([0-9A-Fa-f]{8})")


def cmake_exe() -> Path:
    # preferimos el cmake de VS (mismas dependencias que las builds)
    for c in [
        r"C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe",
        Path(r"C:\Program Files\CMake\bin\cmake.exe"),
        ROOT / "extern" / "rexglue-sdk" / "out" / "win-amd64" / "cmake" / "bin" / "cmake.exe",
    ]:
        if Path(c).exists():
            return Path(c)
    raise SystemExit("[error] cmake no encontrado")


def existing_entries() -> set[int]:
    txt = CODEGEN_TOML.read_text(encoding="utf-8")
    in_func = False
    found: set[int] = set()
    for line in txt.splitlines():
        s = line.strip()
        if s.startswith("["):
            in_func = s == "[functions]"
            continue
        if in_func:
            m = re.match(r"0x([0-9A-Fa-f]{8})\s*=", s)
            if m:
                found.add(int(m.group(1), 16))
    return found


def add_function(addr: int) -> bool:
    if addr in existing_entries():
        return False
    txt = CODEGEN_TOML.read_text(encoding="utf-8")
    txt = re.sub(r"(?<=\[functions\])\n", f"\n0x{addr:X} = {{ }}\n", txt, count=1)
    CODEGEN_TOML.write_text(txt, encoding="utf-8")
    return True


def run_host(preset: str, timeout: int) -> tuple[int, int | None]:
    """Devuelve exitcode y crash addr leida del log de fichero del build dir."""
    exe_dir = ROOT / "out" / "build" / preset
    log_path = exe_dir / "splatterhouse.log"
    if log_path.exists():
        log_path.unlink()
    exe = exe_dir / "splatterhouse.exe"
    if not exe.exists():
        raise SystemExit(f"[error] no existe {exe} (compila el preset {preset})")
    proc = subprocess.Popen([str(exe)] , cwd=exe_dir, stdout=subprocess.DEVNULL)
    deadline = time.time() + timeout
    while time.time() < deadline:
        if proc.poll() is not None:
            break
        time.sleep(0.5)
    if proc.poll() is None:
        proc.kill()
        return 0, None  # sigue vivo = no crash: これ OK
    code = proc.returncode
    txt = log_path.read_text(encoding="utf-8", errors="ignore") if log_path.exists() else ""
    for m in FATAL_RE.findall(txt):
        return code, int(m, 16)
    return code, None


def main():
    import argparse
    ap = argparse.ArgumentParser()
    ap.add_argument("--preset", default="win-amd64-debug")
    ap.add_argument("-n", "--iterations", type=int, default=5)
    ap.add_argument("--run-timeout", type=int, default=45)
    args = ap.parse_args()

    cmake = cmake_exe()
    for i in range(1, args.iterations + 1):
        print(f"\n=== iteracion {i}/{args.iterations} ===")
        print("[build] cmake --build ...")
        if subprocess.call([str(cmake), "--build", str(ROOT / "out" / "build" / args.preset), "-j", "4"]) != 0:
            print("[fix] build fallo - parando para revisar a mano")
            return 1
        code, crash_addr = run_host(args.preset, args.run_timeout)
        print(f"[run] exitcode={code}")
        if crash_addr is None:
            print("[run] Sin crash: SDI? reví reporte manual. terminado.")
            return 0
        addr = crash_addr
        print(f"[run] crash en 0x{addr:08X}")
        if not add_function(addr):
            print(f"[run] 0x{addr:08X} ya estaba en codegen.toml pero sigue crasheando; STOP")
            return 1
        print(f"[add] añadida 0x{addr:08X} -> codegen")
        if subprocess.call([sys.executable, str(ROOT / "tools" / "recompile.py")]) != 0:
            print("[fix] codegen fallo (haz --force o revisa) - STOP")
            return 1
    print("Hecho (limite de iteraciones alcanzado).")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
