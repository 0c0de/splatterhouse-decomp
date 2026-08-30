#!/usr/bin/env bash
set -e
echo "[setup] Splatterhouse Recompiled - Linux setup"
command -v git >/dev/null || { echo "git no encontrado"; exit 1; }
command -v cmake >/dev/null || { echo "cmake no encontrado"; exit 1; }
command -v ninja >/dev/null || echo "[warn] ninja no encontrado, instala ninja-build"
command -v clang >/dev/null && echo "[ok] clang $(clang --version | head -1)" || echo "[info] usando gcc"
if [ -d .git ]; then
  git submodule update --init --recursive
  echo "[ok] submodules"
fi
echo "Para compilar:"
echo "  cmake --preset linux-release && cmake --build build/linux-release -j\$(nproc)"
