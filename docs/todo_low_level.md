Low-Level TODO
==============

- Projectile sprites: [Done] Add `sprite` to projectile defs; set pr.sprite_id and render.
- Fire modes edge cases: Verify burst completion behavior and cooldown interplay; ensure semi-auto only fires on edge.
- Sounds defaults: Add hard fallbacks for pickup/use if keys missing (base:drop/base:ui_confirm). Add explicit fallback chain for `base:ui_super_confirm`.
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
- Add AR fail VFX/SFX tuning; consider small cooldown UI indicator.

FX/Shake
- Expand shake usage to gun panel and ammo indicators (already partially done); add timers and caps.
- Consider user setting to scale shake strength (0..1) as a multiplier (no toggle).

Pickups/Prompts
- [Done] Select best-overlap pickup; single prompt uses active bind.
- [Done] Ground items/guns sized to half character; power-ups smaller.
- Ensure ground guns always resolve sprite from defs if `sprite_id` unset (crate/API spawns verified).
- [Done] Crate size tuned (≈0.45×0.20). Consider sprite-based crates later.
- [Done] Lua `spawn_item`/`spawn_gun` push out of impassable tiles to nearest walkable center.

Ticking
- Add per-instance runtime toggle API for ticking and rate/phase adjust.
- Add compact host lists and cap per-phase executions; expose counters in debug overlay.

HUD details
- Dash slivers left-aligned; allow tuning `slw`/`sgap` in settings for iteration.

Pages/Rooms (new)
- Reduce `EXIT_COUNTDOWN_SECONDS` to 5s; `SCORE_REVIEW_INPUT_DELAY` ≈0.8s.
- Gate HUD/world renders to `MODE_PLAYING` only.
- Stage Review page: heading + metrics; prompt bottom center.
- Next Area page: heading + next-room info (name/type/difficulty%); prompt bottom center.
- Proceed conditions: SPACE or left click after delay.
- Add State metrics counters: damage_dealt, shots_fired/hit, dashes_used, dmg_taken, dmg_shields, plates_gained/lost, time_in_stage, crates_opened, pickups.
- Add Decoration data and renderer pass; y-sort with entities; texture lookup via sprite key.
