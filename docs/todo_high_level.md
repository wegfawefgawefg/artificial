High-Level TODO
===============

- Projectiles: [Done] Sprite rendering from Lua `sprite` with fallback visuals; consider animation frames.
- Sounds: Finalize per-gun/item sound mapping in Lua; add fallbacks/volume controls; namespaced keys only. Add explicit fallback path for `base:ui_super_confirm` to `base:ui_confirm` if missing.
- Namespacing: [Done] Enforced namespaced-only sprite lookup; document conventions in README/mods docs.
- Crates: Expand crate types and Lua-driven placement; safe scatter generation in Lua.
- Guns: Reload/eject timing, heat/cool dynamics; polish mag/reserve UI; per-gun tuning.
- Damage/Triggers: Wire on_damage fully for player/enemy; expose AP/armor properly; integrate plates consistently.
- Modifiers (design ready): Implement registry + attached modifiers with stats pipeline and hooks (defer until after core polish).
- Rendering: Animated sprite playback and projectile sprites using SpriteStore frame data.
- UX & Persistence: Gun detail panel polish; save input settings back to .ini; optional audio settings.
- Cleanup & Tooling: Trim legacy paths; ASan/UBSan preset; small logging helper.

Ticking (Opt-in) Roadmap
- [Done] Per-def opt-in ticks for items/guns with `tick_rate_hz`, `tick_phase` (before/after). Initial host: player inventory.
- [Next] Host registry per system (items/guns/NPCs/projectiles) to avoid scanning non-tickers; register/unregister on state changes.
- [Next] Runtime controls to enable/disable ticking per instance and adjust rate/phase.
- [Next] Per-phase budgets and basic telemetry (executed/spilled counts).

Active Reload and Reload Lifecycle
- [Done] Add active reload with per-gun tuning: `eject_time`, `reload_time`, `ar_pos`, `ar_pos_variance`, `ar_size`, `ar_size_variance`.
- [Done] Global + gun + item hooks: `on_eject`, `on_reload_start`, `on_active_reload`, `on_reload_finish`.
- [Done] Visualize active window band; progress grows from bottom. Remove tilt.
- [Next] Optional VFX/sounds per hook; configurable success/failed SFX per gun. Add explicit fallbacks.

Dash & Movement
- [Done] Dash with stocks and refill; Lua control for max/current; dash hooks.
- [Done] Dash uses WASD 8-way direction and latches during dash.
- [Next] Add per-class modifers (when classes land) to control dash stocks/refill.

HUD/UX
- [Done] Three-row condition bars (Shield/Plates/Health) with counts and fixed width; dash is a fourth row with slivers + refill indicator.
- [Done] Gun UI shows fire mode + burst cadence; content includes Burst Rifle.
- [Done] Pickup UX: best-overlap selection; single prompt using active bind; F is the pickup key.
- [Next] Minor tuning for dash sliver spacing/width; optional labels toggle (Shield/Plates/HP).
