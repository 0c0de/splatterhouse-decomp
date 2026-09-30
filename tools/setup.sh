#!/usr/bin/env bash
set -e
echo "[setup] Splatterhouse Recompiled - Linux setup"
command -v git >/dev/null || { echo "git no encontrado"; exit 1; }
command -v cmake >/dev/null || { echo "cmake no encontrado"; exit 1; }
command -v ninja >/dev/null || echo "[warn] ninja no encontrado, instala ninja-build"
if command -v clang >/dev/null; then
  echo "[ok] clang $(clang --version | head -1)"
  v=$(clang --version | head -1 | sed -n 's/.*version \([0-9]\+\)\..*/\1/p')
  if [ "$v" -lt 18 ]; then
    echo "[warn] ReXGlue requiere Clang 18+ (instala llvm-20 o superior)"
  fi
else
  echo "[warn] ReXGlue requiere Clang 18+ (apt install clang libgtk-3-dev)"
fi
if [ -d .git ]; then
  git submodule update --init extern/rexglue-sdk
  echo "[ok] rexglue-sdk"
fi
echo "Para compilar:"
echo "  cmake --preset linux-amd64-release && cmake --build out/build/linux-amd64-release -j\$(nproc)"
