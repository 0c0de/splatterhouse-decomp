#!/usr/bin/env python3
"""
Busca las 8 funciones __rest/save gpr/fpr/vmx en el XEX de Splatterhouse.
Patrones sacados del README de XenonRecomp (byte patterns en BE).
Uso: python tools/find_restores.py game/default.dec.xex
"""
import sys
from pathlib import Path

# Patrones BE tal cual aparecen en el XEX (big-endian)
PATTERNS = {
    "restgprlr_14": bytes.fromhex("E9 C1 FF 68"),  # ld r14, -0x98(r1)
    "savegprlr_14": bytes.fromhex("F9 C1 FF 68"),  # std r14, -0x98(r1)
    "restfpr_14":   bytes.fromhex("C9 CC FF 70"),  # lfd f14, -0x90(r12)
    "savefpr_14":   bytes.fromhex("D9 CC FF 70"),  # stfd f14, -0x90(r12)
    "restvmx_14":   bytes.fromhex("39 60 FE E0 7D CB 60 CE"), # li r11,-0x120; lvx v14,r11,r12 ...
    "savevmx_14":   bytes.fromhex("39 60 FE E0 7D CB 61 CE"),
    "restvmx_64":   bytes.fromhex("39 60 FC 00 10 0B 60 CB"), # li r11,-0x400; lvx128 v64 ...
    "savevmx_64":   bytes.fromhex("39 60 FC 00 10 0B 61 CB"),
}

# Fallback patterns más cortos por si cambian
FALLBACK = {
    "restgprlr_14_alt": bytes.fromhex("E9 C1 FF 68 E9 E1 FF 70"),
    "savegprlr_14_alt": bytes.fromhex("F9 C1 FF 68 F9 E1 FF 70"),
}

def find_in_file(path: Path):
    data = path.read_bytes()
    print(f"[find] {path} ({len(data)} bytes)")
    # Intenta sacar base del ppc_config.h si existe
    base = 0x82000000  # típico XEX base
    ppc_config = Path("recompiled/ppc_config.h")
    if ppc_config.exists():
        txt = ppc_config.read_text(errors="ignore")
        import re
        m = re.search(r"#define PPC_IMAGE_BASE 0x([0-9A-Fa-f]+)", txt)
        if m:
            base = int(m.group(1), 16)
            print(f"[find] PPC_IMAGE_BASE detectado: 0x{base:X} (de recompiled/ppc_config.h)")

    results = {}
    for name, pat in PATTERNS.items():
        idx = data.find(pat)
        if idx != -1:
            # VA aproximada: base + offset ajustado por header.
            # Para XEX el header no es trivial, pero el offset en fichero suele ser cercano a VA-base.
            # Reportamos ambos: file offset y VA estimada.
            # Buscamos todos los matches, no solo el primero
            offs = []
            start = 0
            while True:
                i = data.find(pat, start)
                if i == -1:
                    break
                offs.append(i)
                start = i+1
            vas = [f"0x{base + o:X} (file+0x{o:X})" for o in offs]
            print(f"  {name:15s} -> {', '.join(vas)}  [pattern {pat.hex()}]")
            results[name] = base + offs[0]
        else:
            print(f"  {name:15s} -> NO ENCONTRADO (pattern {pat.hex()})")

    print("\n# Copia esto a config/recomp.toml [main]:")
    for name in PATTERNS:
        if name in results:
            print(f'{name}_address = 0x{results[name]:X}')
        else:
            print(f'# {name}_address = 0x0  # NO ENCONTRADO - deja comentado o pon 0')

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print(f"Uso: {sys.argv[0]} game/default.dec.xex")
        sys.exit(1)
    find_in_file(Path(sys.argv[1]))
