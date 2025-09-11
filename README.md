Gub C++ Scaffold
=================

Minimal C++20 project using SDL2 and GLM, with CMake.

Build
-----

- Requirements: CMake 3.20+, a C++20 compiler, SDL2 (dev package), GLM (header-only).

Debian quickstart
-----------------

- Install deps:

```
bash scripts/setup_debian.sh
```

- Configure and build:

```
# Configure and build normally:
cmake --build build -j
./build/gub

# Or use the helper:
bash scripts/run.sh
```

Example OS setup
----------------

- Ubuntu/Debian (recent): `sudo apt install libsdl2-dev libglm-dev`
- Arch: `sudo pacman -S sdl3 glm`
- macOS (Homebrew): `brew install sdl3 glm`
- Windows (vcpkg):
  - Install vcpkg and integrate toolchain
  - `vcpkg install sdl2 glm`
  - Configure with: `-DCMAKE_TOOLCHAIN_FILE="/path/to/vcpkg.cmake"`

Configure & build
-----------------

Commands:

```
cmake -S . -B build            # configure (fails if deps missing)
cmake --build build -j         # build
./build/gub                    # run (Linux/macOS)
```

If you want configure to succeed even when SDL2/GLM aren’t found (for CI or quick syntax checks), pass:

```
cmake -S . -B build -DGUB_REQUIRE_DEPS=OFF
```

CMake dependency discovery
--------------------------

- SDL2: prefers CMake config package `SDL2::SDL2`. Falls back to `pkg-config sdl2` if available.
- GLM: prefers CMake config package `glm::glm`. Falls back to `pkg-config glm`.

If CMake can’t find them, set package dirs explicitly, e.g.:

```
cmake -S . -B build -DSDL2_DIR=/path/to/SDL2/lib/cmake/SDL2 -Dglm_DIR=/path/to/glm
```

What’s next
-----------

- You can now drop in engine sources under `src/`.
- We’ll wire in Lua FFI and data-driven tables once the base is compiling and the window loop runs.

Source layout (engine)
----------------------

- `src/main.cpp`: App entry; window + main loop, input build, fixed 60 Hz sim tick, page mode transitions. Delegates to `render_frame(...)` and `sim_*` helpers.
- `src/render.hpp/cpp`: All rendering, including world (tiles/entities/pickups/items/guns), HUD (reticle, bars, inventory), panels (character, equipped gun, ground inspect), and pages (Score Review with animated metrics + sounds, Next Stage page). Call: `render_frame(state, gfx, dt_sec, binds, projectiles)`. Uses global `g_textures` and `gfx.ui_font`.
- `src/sim.hpp/cpp`: Simulation helpers called from the fixed-step loop: movement/collision, shield regen + reloads, drop mode, number-row inventory handling (equip/select), ground repulsion, crate open progress, projectile stepping.
- `src/room.hpp/cpp`: Room generation and spawn helpers.
- `src/luamgr.*`: Lua content registration and calls.
- `src/*`: Remaining modules for inputs, textures, sprites, config, etc.

Mods and hot reload
-------------------

- Base mod scaffold lives under `mods/base/` with `info.toml` and empty `graphics/`, `sounds/`, `music/`, `scripts/`.
- A lightweight `ModsManager` discovers mods and polls for changes every ~0.5s.
- Sprite registry stub (`SpriteIdRegistry`) rebuilds automatically when files in `mods/*/graphics/` change.
- Script changes are detected and logged; behavior reload is stubbed until Lua is integrated.
