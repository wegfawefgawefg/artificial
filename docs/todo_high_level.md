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
- Rooms/Pages: Split world render from pages; add Stage Review and Next Area pages with metrics and next-room details; click/space to continue.
- Rooms/Rotation: Add 3-room loop (Prep → Arena → Gauntlet) with simple generators.
- Decor: Add non-interactable decorations (z-sorted by Y); Lua placement API.

Entity & Ammo Systems
- Entity Type Defs (Lua): register entity types (player/classes/NPCs) with base stats including movement inaccuracy dynamics (inc/decay/max), accuracy, shield/armor, etc.; assign entities a type.
- Ammo Type Defs (Lua): define ammunition families + variants with stats/modifiers (damage, AP, speed, falloff, crit mods, shield/headshot/boss modifiers, ricochet/shrapnel profiles, bounce caps, range/lifespan/acceleration); expose curves for distance→multiplier (falloff).
- Gun ↔ Ammo Compatibility: guns list compatible ammo types; inventory/loader picks the active ammo; UI shows current ammo and effects.
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

Docs
- Modding Guide: author a gamer-friendly how-to covering mod tree layout, Lua registration tables (guns, ammo, items, crates), naming conventions, and hot reload. Link it from README.

Steam / Distribution
- Steam Page Checklist: capsule art (all sizes), screenshots, trailer, description/features, tags, pricing, age rating, achievements (if any), depots/branches setup, store localization plan.
- Steamworks Integration: overlays where relevant, rich presence, achievements (optional), Cloud (settings saves) — scope later.
- Depots/Builds: CI artifacts for Windows and macOS; upload to Steam via build scripts.

Multiplayer
- Architecture Decision: pick approach (lockstep vs client/authoritative server). Prototype small sync testbed first.
- Networking (Steam): Steam Networking Sockets + lobbies/matchmaking; invites and friend join flow.
- Networking (Non‑Steam): ENet/UDP fallback with NAT traversal; compatible matchmaking directory.
- Dedicated Server: headless build target + minimal server loop; Steam server listing + non‑Steam directory.
- Friends: Steam friends presence and join/invite; session privacy controls.

Platforms / Builds
- Windows build: validated release profile + runtime deps; installer/zip packaging.
- macOS build: validated release profile + codesigning/notarization plan (later).
- Linux build: continue dev preset; add release preset when ready.

Modifiers System
- Status: planned/design‑ready; not yet started. Implement registry + stat pipeline + durations/stacking with hooks after core polish.
