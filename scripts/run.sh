#!/usr/bin/env bash
set -euo pipefail

# Prefer X11 on i3 unless user overrides SDL_VIDEODRIVER
if [ -n "${DISPLAY:-}" ] && [ -z "${SDL_VIDEODRIVER:-}" ]; then
  export SDL_VIDEODRIVER=x11
fi

if [ ! -d build ]; then
  cmake --preset dev || true
fi

cmake --build --preset dev -j
"$(dirname "$0")"/../build/gub
