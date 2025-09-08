High-Level TODO
===============

- Projectiles: Add sprite rendering for projectiles (optional `sprite` on projectile defs) and visibility polish.
- Sounds: Finalize per-gun/item sound mapping in Lua; global fallbacks and volume controls; namespaced keys only.
- Namespacing: Enforce namespaced sprite lookup exclusively once base assets are in; document conventions.
- Crates: Expand crate types and Lua-driven placement; safe scatter generation in Lua.
- Guns: Reload/eject timing, heat/cool dynamics; polish mag/reserve UI; per-gun tuning.
- Damage/Triggers: Wire on_damage fully for player/enemy; expose AP/armor properly; integrate plates consistently.
- Modifiers (design ready): Implement registry + attached modifiers with stats pipeline and hooks (defer until after core polish).
- Rendering: Animated sprite playback and projectile sprites using SpriteStore frame data.
- UX & Persistence: Gun detail panel polish; save input settings back to .ini; optional audio settings.
- Cleanup & Tooling: Trim legacy paths; ASan/UBSan preset; small logging helper.
