#!/usr/bin/env bash
set -euo pipefail

PREFIX="${HOME}/.local"
SDL3_CMAKE_DIR="${PREFIX}/lib/cmake/SDL3"

echo "[gub] Updating apt and installing base toolchain + glm + SDL2..."
sudo apt update
sudo apt install -y \
  build-essential cmake ninja-build pkg-config git gdb \
  libglm-dev libsdl2-dev

echo "[gub] Installed dependencies via apt. You should be set."
