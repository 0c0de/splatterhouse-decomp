"""
Encuentra direcciones de funcion NO registradas referenciadas desde data
(vtables / punteros de funcion) usando el dump de la imagen guest.

Requiere el dump creado por la app (cvar dump_guest=1):
  set REXCVAR_dump_guest=1
  out\\build\\win-amd64-release\\splatterhouse.exe
(o dump_guest = true en splatterhouse.toml)

Uso:
  python tools/find_unregistered.py            # informe
  python tools/find_unregistered.py --apply    # añade las entradas a config/codegen.toml
"""
import re
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
DUMP = Path("out/build/win-amd64-release/guest_image.bin")
if not DUMP.exists():
    DUMP = Path("out/build/win-amd64-debug/guest_image.bin")
INIT_CPP = ROOT / "generated" / "splatterhouse_init.cpp"
CODEGEN_TOML = ROOT / "config" / "codegen.toml"

registered = set(
    int(m.group(1), 16)
    for m in re.finditer(r"\{\s*0x([0-9A-Fa-f]{8}),", INIT_CPP.read_text(encoding="utf-8", errors="ignore"))
)
pch = (ROOT / "generated" / "splatterhouse_pch.h").read_text(encoding="utf-8", errors="ignore")
img_base = int(re.search(r"REX_IMAGE_BASE 0x([0-9A-Fa-f]+)", pch).group(1), 16)
img_size = int(re.search(r"REX_IMAGE_SIZE 0x([0-9A-Fa-f]+)", pch).group(1), 16)
code_base = int(re.search(r"REX_CODE_BASE 0x([0-9A-Fa-f]+)", pch).group(1), 16)
code_size = int(re.search(r"REX_CODE_SIZE 0x([0-9A-Fa-f]+)", pch).group(1), 16)
code_end = code_base + code_size

data = DUMP.read_bytes()
print(f"dump: {DUMP} ({len(data)/1024/1024:.1f} MB, base {img_base:#x})")
code_lo = code_base - img_base
data_region = data[:code_lo]

# Referencias desde data: BE32 valores dentro del rango [code_base, code_end)
candidates = set()
counts = {}
for off in range(0, code_lo - 4, 4):
    (w,) = struct.unpack(">I", data_region[off : off + 4])
    if code_base <= w < code_end:
        # Filtra: el valor debe ser una direccion 4-align
        if w & 3:
            continue
        candidates.add(w)
        counts[w] = counts.get(w, 0) + 1

missing = sorted(a for a in candidates if a not in registered)
print(f"referencias en data que apuntan a codigo: {len(candidates)}")
print(f"no registradas: {len(missing)}")

def prologue(addr):
    off = addr - img_base
    if off + 8 > len(data):
        return "?"
    w, = struct.unpack(">I", data[off : off + 4])
    opcode = w >> 26
    if (w & 0xFFFF0000) == 0x94210000: return "stwu r1"   # prologo stack frame
    if opcode == 15: return "addis"
    if opcode == 18: return "b/bl"
    if w == 0x4E800020: return "blr"
    if opcode == 20 or opcode == 24: return "arith"
    return f"{opcode}"

for a in missing[:60]:
    print(f"  {a:#010x}  (x{counts.get(a,1)})  insn={prologue(a)}")
if len(missing) > 60:
    print(f"  ... y {len(missing)-60} mas")

if "--apply" in sys.argv:
    if not missing:
        print("nada que añadir")
        sys.exit(0)
    toml = CODEGEN_TOML.read_text(encoding="utf-8")
    toml = re.sub(r"(?<=\[functions\])\n", "\n" +
        "\n".join(f"0x{a:X} = {{ }}" for a in missing) + "\n", toml, 1)
    CODEGEN_TOML.write_text(toml, encoding="utf-8")
    print(f"aplicado: {len(missing)} entradas añadidas a config/codegen.toml")
