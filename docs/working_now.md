Working Now: Main Loop Refactor
================================

Objective
- Reduce `src/main.cpp` to init → loop (sim_step, render_frame) → cleanup.
- Preserve EXACT behavior. No rendering or game logic should remain in main.

Completed so far
- Globals refactor across input, sim, room helpers, LuaManager; all call sites updated.
- Texture/UI helpers now read globals (`TextureStore::load_all()`, `ui_draw_kv_line`).
- Introduced `sim_step()` in `src/sim.hpp/cpp` and began moving per-frame logic:
  - Event polling, dt computation, build_inputs
  - Alert aging/purge and mods hot-reload polling
  - Fixed-timestep updates: pre-physics ticks, movement/collision, shield/reload progress, pickups, drop mode, number-row actions, metrics/lockouts, exit tile transitions + score review prep, camera follow, crate progression

Next steps (to finish before removing logic from main)
- Move the remaining active-reload input handling and firing/projectile spawning fully into `sim_step()` (WIP code is already staged in `sim.cpp`).
- Replace the body of the main loop with:
  - `sim_step();`
  - `if (!arg_headless) render_frame();`
  - FPS title update using `g_state->dt`
  - `--frames` countdown exit check

Testing
- Build: `cmake -S . -B build && cmake --build build -j`
- Headless smoke: `./build/artificial --headless --frames=1` (alias: `./build/arti`)
- Manual run to verify gameplay and ensure no behavior regressions.

Notes
- No rendering will move into `sim_step()`; all rendering remains in `render_frame()`.
- Keep CLI args behavior (`--headless`, `--frames`) unchanged.
