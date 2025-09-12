Developer Setup
===============

Requirements
------------

- CMake 3.20+
- C++20 compiler
- SDL2, GLM
- Lua 5.4 (dev headers)
- SDL2_image, SDL2_ttf, SDL2_mixer

Linux (Debian/Ubuntu)
---------------------

```
bash scripts/setup_debian.sh
cmake --preset dev && cmake --build --preset dev -j
./build/artificial     # alias: ./build/arti
```

Windows (vcpkg)
---------------

1) Install and integrate vcpkg.
2) Install deps: `vcpkg install sdl2 glm lua sdl2-image sdl2-ttf sdl2-mixer`.
3) Configure with the toolchain file, e.g. `-DCMAKE_TOOLCHAIN_FILE="/path/to/vcpkg.cmake"`.

macOS (Homebrew)
----------------

```
brew install sdl2 glm lua sdl2_image sdl2_ttf sdl2_mixer
cmake --preset dev && cmake --build --preset dev -j
./build/artificial
```

CMake Notes
-----------

- Presets: `CMakePresets.json` provides a `dev` preset (Debug, strict warnings).
- Dependency discovery prefers CMake packages and falls back to pkg‑config when available.
- Options (kept for backward compatibility in the codebase):
  - `GUB_REQUIRE_DEPS` (ON by default): fail configure if deps are missing.
  - `GUB_STRICT` and `GUB_WARN_AS_ERROR`: enable strict warnings and treat warnings as errors.

Troubleshooting
---------------

- If CMake can’t find a package, set package dirs explicitly, e.g.:
  - `-DSDL2_DIR=/path/to/SDL2/lib/cmake/SDL2`
  - `-Dglm_DIR=/path/to/glm`
- Run script prefers X11 on i3 unless `SDL_VIDEODRIVER` is set.

