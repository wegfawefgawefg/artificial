Ticking System Status
=====================

Summary
-------
- Implemented opt-in ticking for items and guns with per-def rate and phase:
  - `tick_rate_hz`: 0 disables; >0 enables ticking with an accumulator.
  - `tick_phase`: "before" or "after" physics (defaults to "after").
- Engine only iterates ticking hosts (currently the player’s inventory), not every entity.
- Accumulator-based scheduler with a per-frame hard cap (currently ~4000 calls); spill to next frame.
- Items use `on_tick(dt)`; guns use `on_step()`.

Incomplete / Known Gaps
-----------------------
- Only the player’s inventory participates. No ticking yet for NPCs, projectiles, or stage systems.
- No runtime API to enable/disable ticking per instance or adjust rate/phase dynamically.
- No central registry to compact/track ticking hosts across systems (we iterate inventory directly).
- No per-phase budget split; the single cap is shared across before/after phases.

Proposed Next Steps
-------------------
1) Host Registry
   - Maintain compact lists of ticking hosts per system (items, guns, projectiles, NPCs).
   - Register/unregister on state changes to avoid scanning non-tickers.
2) Runtime Controls
   - Lua API to toggle ticking on instances (enable/disable), adjust `rate_hz` and `phase` at runtime.
3) Phase Budgets
   - Separate caps for BeforePhysics and AfterPhysics (e.g., 2k each) to avoid starvation.
4) Broaden Scope Carefully
   - Allow opt-in ticking for select NPCs or projectiles (homing, auras), never blanket per-entity.
5) Telemetry
   - Lightweight counters per frame: total tick candidates, executed, spilled, by system/phase.

Guidance
--------
- Prefer event/phase hooks and timers over ticks. Use ticks only for genuine continuous behaviors
  (DOT auras, homing) and only on the few instances that require it.
