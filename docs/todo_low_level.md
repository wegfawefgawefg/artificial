Low-Level TODO
==============

- Projectile sprites: Add `sprite` to projectile defs; set pr.sprite_id and render. Enlarge/contrast for visibility during testing.
- Fire modes edge cases: Verify burst completion behavior and cooldown interplay; ensure semi-auto only fires on edge.
- Sounds defaults: Add hard fallbacks for pickup/use if keys missing (base:drop/base:ui_confirm).
- Namespaced sprites: Audit all lookups to avoid non-namespaced fallbacks; update base content as needed.
- Crate safe placement: Expose safe spawn utility to Lua (done); ensure non-overlap with impassable tiles and other crates if needed.
- on_damage wiring: Route damage to player/NPC; invoke Lua item on_damage(attacker_ap) and prepare projectile metadata for hooks.
- Gun UI: Show per-gun sounds and fire mode info; refine mag/reserve bar scaling.
- Heat/reload timing: Add proper reload/eject timing and optional heat/overheat model.
- Settings: Save input bindings/options back to config/input.ini.
- Enemy drops: Optionally move to per-enemy drop tables via Lua defs.
- Tooling: Add ASan/UBSan preset; minimal logger for asset/script reload.
