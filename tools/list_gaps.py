"""
Lista los huecos de la tabla de funciones del proyecto (direcciones guest sin
funcion registrada en PPCFuncMappings).

Uso:
  python tools/list_gaps.py                       # todos los huecos del modulo
  python tools/list_gaps.py 0x82F2D000 0x82F75818 # solo en un rango
"""
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
INIT_CPP = ROOT / "generated" / "splatterhouse_init.cpp"
MAPPING_RE = re.compile(r"\{\s*0x([0-9A-Fa-f]{8}),")

args = [a for a in sys.argv[1:]]
lo = int(args[0], 16) if len(args) > 0 else None
hi = int(args[1], 16) if len(args) > 1 else None

addrs = [int(m.group(1), 16) for m in MAPPING_RE.finditer(INIT_CPP.read_text(encoding="utf-8", errors="ignore"))]
addrs = sorted(set(a for a in addrs if 0x80000000 <= a < 0xC0000000))
print(f"{len(addrs)} funciones registradas")

code_entries = []
# code base/size desde el pch
pch = (ROOT / "generated" / "splatterhouse_pch.h").read_text(encoding="utf-8", errors="ignore")
base = int(re.search(r"REX_CODE_BASE 0x([0-9A-Fa-f]+)", pch).group(1), 16)
cbeg, cend = base, base + int(re.search(r"REX_CODE_SIZE 0x([0-9A-Fa-f]+)", pch).group(1), 16)
print(f"region de codigo: {cbeg:#x} - {cend:#x}\n")

prev = None
gaps = []
for a in addrs + [cend]:
    if lo is not None and (a < lo or (prev is not None and prev < lo)):
        prev = a
        continue
    if a > cend and prev is not None:
        break
    if prev is not None and a != prev:
        # hueco entre prev y a
        g_start, g_end = prev, a
        if lo is not None and g_end <= lo:
            prev = a
            continue
        gaps.append((max(g_start, cbeg), min(g_end, cend)))
    prev = a

total_gap = sum(e - s for s, e in gaps if e > s)
print(f"{len(gaps)} huecos distintos\n")
if lo is not None and hi is not None:
    show = [g for g in gaps if g[1] > lo and g[0] < hi]
else:
    show = [g for g in gaps if g[1] > g[0]]
for s, e in show[:80]:
    print(f"  {s:#010x} - {e:#010x}  ({e-s} bytes)")

if len(show) > 80:
    print(f"... y {len(show)-80} mas")
