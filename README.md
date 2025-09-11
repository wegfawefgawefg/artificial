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
- `src/render.hpp/cpp`: All rendering, including world (tiles/entities/pickups/items/guns), HUD (reticle, bars, inventory), panels (character, equipped gun, ground inspect), and pages (Score Review with animated metrics + sounds, Next Stage page). Call: `render_frame(gfx, dt_sec, projectiles)`. Uses global `g_textures` and `gfx.ui_font`.
- `src/sim.hpp/cpp`: Simulation helpers called from the fixed-step loop: movement/collision, shield regen + reloads, drop mode, number-row inventory handling (equip/select), ground repulsion, crate open progress, projectile stepping.
- `src/room.hpp/cpp`: Room generation and spawn helpers.
- `src/luamgr.*`: Lua content registration and calls.
- `src/*`: Remaining modules for inputs, textures, sprites, config, etc.

Global subsystems
-----------------

These are exposed via `src/globals.hpp` for convenience and to simplify wiring:

- `g_gfx`: Graphics (owns `SDL_Window* window`, `SDL_Renderer* renderer`, `TTF_Font* ui_font`). Initialize with `init_graphics(gfx, headless, title, w, h)`, destroy with `shutdown_graphics(gfx)`.
- `g_audio`: Audio (owns `SoundStore sounds`). Initialize via `g_audio->init()`, shutdown via `g_audio->shutdown()`.
- `g_mods`: ModsManager for discovery and hot-reload polling.
- `g_sprite_ids`: SpriteIdRegistry for name→id lookups.
- `g_sprite_store`: SpriteStore for sprite metadata used by texture loading.
- `g_textures`: TextureStore for sprite textures; rendering fetches textures from here.
- `g_binds` / `g_input`: InputBindings and InputContext live in main and are referenced by modules that need to read current input or display key prompts.
- `g_settings`: RuntimeSettings (e.g., camera follow factor, exit countdown seconds) with sensible defaults.
- `g_state`: Game State; simulation and rendering read/write through this.

Notes
- Headless mode is opt-in only via `--headless`. Window/renderer are not created when headless.
- SDL dummy driver is never used unless headless is explicitly requested.

Refactor to globals
-------------------

We are migrating frequently shared resources to global singletons (declared in `src/globals.hpp`) and updating APIs to stop passing them around. This reduces boilerplate in function signatures and call sites and makes evolution easier during the refactor.

Completed so far:
- Graphics: `g_gfx` (window, renderer, font) with `init_graphics`/`shutdown_graphics`.
- Audio: `g_audio->sounds` for playback.
- Mods: `g_mods` for discovery and hot reload polling.
- Sprites/Textures: `g_sprite_ids`, `g_sprite_store`, `g_textures`.
- Inputs: `g_binds`, `g_input`.
- Runtime settings: `g_settings` (camera follow factor, exit countdown, etc.).
- Game state: `g_state` used by sim/render/input; `State` also owns `projectiles` now, and per-frame `dt` is set on `g_state` each frame.
- Function signatures updated to use globals internally:
  - `render_frame()` (no args)
  - `generate_room()` (no args)
  - `sim_step_projectiles()` (no args)
  - `build_inputs(...)` (removed `dt` parameter; reads from `g_state->dt`)

Upcoming candidates:
- Remove `Graphics&` parameters from helpers like `build_inputs(...)` and `sim_move_and_collide(...)`; read from `g_gfx` directly.
- Expand `RuntimeSettings` (g_settings) to cover more tweakables (dash cooldown, spread dynamics), so fewer constants are hardcoded.
- Consolidate Lua-facing calls to use `g_state` internally where they still accept `State&` by parameter (optional and incremental).

Mods and hot reload
-------------------

- Base mod scaffold lives under `mods/base/` with `info.toml` and empty `graphics/`, `sounds/`, `music/`, `scripts/`.
- A lightweight `ModsManager` discovers mods and polls for changes every ~0.5s.
- Sprite registry stub (`SpriteIdRegistry`) rebuilds automatically when files in `mods/*/graphics/` change.
- Script changes are detected and logged; behavior reload is stubbed until Lua is integrated.
