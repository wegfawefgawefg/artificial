Technical Overview
==================

Source Layout
-------------

- `src/main.cpp`: App entry; window + main loop, fixed‑timestep sim, page transitions.
- `src/render.hpp/cpp`: World/HUD/panels/pages rendering; uses `g_textures`, `g_gfx->ui_font`.
- `src/sim.hpp/cpp`: Simulation helpers: movement/collision, shield regen/reloads, drop mode, inventory number‑row, ground repulsion, crate open, projectile stepping.
- `src/room.hpp/cpp`: Room generation and spawn helpers.
- `src/luamgr.*`: Lua content registration/calls.
- `src/*`: Inputs, textures, sprites, config, audio, etc.

Globals
-------

Declared in `src/globals.hpp` for convenience during development:

- `g_gfx`: SDL window/renderer, UI font. `init_graphics(...)` / `shutdown_graphics(...)`.
- `g_audio`: `SoundStore` playback; `init()` / `shutdown()`.
- `g_mods`: discovery + hot‑reload polling.
- `g_sprite_ids` / `g_sprite_store` / `g_textures`: sprite ID registry, metadata, textures.
- `g_binds` / `g_input`: input bindings and current frame inputs.
- `g_settings`: runtime tweakables (camera follow factor, exit countdown, etc.).
- `g_state`: full game state; also owns `projectiles` and per‑frame `dt`.

Refactor Notes
--------------

- Many helpers now read globals directly to reduce parameter boilerplate.
- Examples updated: `render_frame()`, `generate_room()`, `sim_step_projectiles()`, `build_inputs()`.

Modding and Hot Reload
----------------------

- Base scaffold under `mods/base/` with `info.toml` and empty `graphics/`, `sounds/`, `music/`, `scripts/`.
- Mods are discovered and polled for changes; sprite data rebuilds on asset changes.
- Lua: guns/ammo/items/crates registration; hooks fire on events (e.g., on_shoot, on_tick, on_step).

