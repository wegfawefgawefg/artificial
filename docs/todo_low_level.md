Low-Level TODO
==============

- Projectile sprites: [Done] Add `sprite` to projectile defs; set pr.sprite_id and render.
- Fire modes edge cases: Verify burst completion behavior and cooldown interplay; ensure semi-auto only fires on edge.
- Sounds defaults: Add hard fallbacks for pickup/use if keys missing (base:drop/base:ui_confirm).
- Namespaced sprites: [Done] Audit all lookups to avoid non-namespaced fallbacks.
- Crate safe placement: Expose safe spawn utility to Lua (done); ensure non-overlap with impassable tiles and other crates if needed.
- on_damage wiring: Route damage to player/NPC; invoke Lua item on_damage(attacker_ap) and prepare projectile metadata for hooks.
- Gun UI: Show per-gun sounds and fire mode info; refine mag/reserve bar scaling.
- Heat/reload timing: [Done] Per-gun eject/reload timings and active reload windows. [Next] Optional heat/overheat model.
- Settings: Save input bindings/options back to config/input.ini.
- Enemy drops: Optionally move to per-enemy drop tables via Lua defs.
- Tooling: Add ASan/UBSan preset; minimal logger for asset/script reload.

Active Reload polish
- Add fallback SFX chain for success (super_confirm -> confirm -> generic).
- Color and thickness tuning for active window and progress rect.
- Expose per-gun SFX fields for `on_active_reload`, `on_reload_start`, `on_reload_finish`.

FX/Shake
- Expand shake usage to gun panel and ammo indicators (already partially done); add timers and caps.
- Consider user setting to scale shake strength (0..1) as a multiplier (no toggle).
