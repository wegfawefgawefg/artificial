Technical Overview
==================

Source Layout
-------------

- `src/main.cpp`: App entry; window + main loop, fixed‑timestep sim, page transitions.
- `src/render.hpp/cpp`: World/HUD/panels/pages rendering; uses `graphics->renderer` and `graphics->ui_font`.
- `src/sim.hpp/cpp`: Simulation helpers: movement/collision, shield regen/reloads, drop mode, inventory number‑row, ground repulsion, crate open, projectile stepping.
- `src/room.hpp/cpp`: Room generation and spawn helpers.
- `src/luamgr.*`: Lua content registration/calls (sol2‑only; protected hooks).
- `src/graphics.hpp/cpp`: Graphics init/shutdown + asset helpers.
- `src/audio.hpp/cpp`: Global audio store + helpers.
- `src/*`: Inputs, sprites, config, etc.

Globals
-------

Declared in `src/globals.hpp`:

- `graphics`: SDL window/renderer/font; owns sprite registry/defs/textures and camera.
- `audio`: global audio store; `init_audio()`, `cleanup_audio()`, `play_sound(...)`, `load_sound(...)`, `load_mod_sounds(...)`.
- `mm`: discovery + hot‑reload polling.
- `lua_mgr`: sol2 Lua manager.
- `binds` / `input_state`: input bindings and current frame inputs.
- `g_settings`: runtime tweakables.
- `state`: full game state; owns `projectiles` and per‑frame `dt`.

Refactor Notes
--------------

- Many helpers now read globals directly to reduce parameter boilerplate.
- Examples updated: `render_frame()`, `generate_room()`, `sim_step_projectiles()`, `build_inputs()`.

Modding and Hot Reload
----------------------

- Base scaffold under `mods/base/` with `info.toml` and empty `graphics/`, `sounds/`, `music/`, `scripts/`.
- Mods are discovered and polled for changes; sprite data rebuilds on asset changes.
- Lua: guns/ammo/items/crates registration; hooks fire on events (e.g., on_shoot, on_tick, on_step). sol2 only.

Assets (Graphics)
-----------------

- Graphics owns sprite registry, sprite definitions, and textures:
  - Registry: `graphics->sprite_name_to_id`, `graphics->sprite_id_to_name`.
  - Sprite defs: `graphics->sprite_defs_by_id`.
  - Textures: `graphics->textures_by_id`.
- Helpers (free functions, use global `graphics`):
  - Registry: `rebuild_sprite_registry(names)`, `add_or_get_sprite_id(name)`, `try_get_sprite_id(name)`.
  - Sprites: `rebuild_sprites(defs)`, `get_sprite_def_by_id(id)`, `try_get_sprite_def(name)`.
  - Textures: `clear_textures()`, `load_all_textures()`, `get_texture(id)`.

Audio
-----

- Global `audio` with helpers: `init_audio()`, `cleanup_audio()`, `load_sound(key,path)`, `play_sound(key,...)`, `load_mod_sounds(root)`.
