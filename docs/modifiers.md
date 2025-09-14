Modifier System Design (Draft)
==============================

Goals
-----
- Allow arbitrary, composable modifiers defined in Lua to affect gameplay at multiple scopes (player, struct, gun, item, projectile, enemy).
- Support both stat changes (e.g., +move_speed, reload_mult=0.8) and behavioral hooks (e.g., on_kill 20% explode, on_hit apply poison).
- Compose many modifiers; some may cancel out (e.g., heavy + light variants) or stack with durations.
- Deterministic, data-driven application order with stable results; predictable merging/splitting (e.g., items with different mods don’t stack).

Core Concepts
-------------
- Modifier: Named bundle with fields and hooks.
  - id/name: string key
  - tags: set of strings (for grouping/conflicts)
  - stats: key→value map and/or multiplicative scalars (e.g., {damage_mult=1.25, reload_mult=0.8})
  - hooks: optional callbacks (in Lua) for events (see Hooks section)
  - constraints: optional rules (e.g., mutually_exclusive_with={"light"}, max_stacks=1)
- Attachments:
  - Player modifiers (struct, perks, powerups)
  - Gun modifiers (variants, roll-based affixes)
  - Item modifiers (upgrades, flavored variants)
  - Projectile modifiers (spawned from gun/player/item context)
- Modifier Cluster: Named set of modifiers (e.g., "heavy" cluster = {heavy_reload, heavy_move, heavy_kick}). Apply cluster by name.

Hooks (Scope-aware)
-------------------
- Global structure, called with context describing source and target:
  - on_projectile_spawn(ctx)
  - on_projectile_hit_entity(ctx, target)
  - on_projectile_hit_tile(ctx, tile)
  - on_kill(ctx, target)
  - on_damage_dealt(ctx, target, amount)
  - on_damage_taken(ctx, source, amount)
  - on_reload(ctx)
  - on_shoot(ctx)
  - on_tick(ctx, dt)
- Context (`ctx`) contains:
  - source: { player_vid?, gun_vid?, item_vid?, projectile_vid?, enemy_vid? }
  - def_types: { gun_type?, item_type?, projectile_type? }
  - stacks, durations, modifier ids in effect at each scope

Evaluation and Composition
--------------------------
- Stats pipeline builds an EffectiveStats/Params view at use-site (shoot/reload/move):
  1) Start from base (gun/item/player)
  2) Apply additive modifiers from all scopes (ordered: player → struct → item → gun)
  3) Apply multiplicative modifiers (same order)
  4) Clamp where needed (e.g., min reload time)
- Hooks collect from all scopes and execute in defined order; allow early out when requested.
- Conflicts: if two modifiers have mutually exclusive tags, last-applied wins or skip application based on rule.

Instances and Stacking
----------------------
- Items/Guns have instance modifier lists (VID → array of modifier ids + stack counts + durations). Items with different modifier lists or timers do not stack.
- Player/Class have persistent modifier lists.
- Projectiles inherit a snapshot of computed modifier effects/flags at spawn (e.g., bouncy, explosive).

Lua API (Sketch)
----------------
- register_modifier{ id, tags=[...], stats={...}, mults={...}, constraints={...}, hooks={...} }
- add_modifier(scope, id, opts) → applies to player/gun/item/enemy with stacks/duration
- remove_modifier(scope, id)
- define cluster: register_modifier_cluster{ id, modifiers=[...] }
- apply cluster: add_modifier_cluster(scope, id)

Implementation Plan
-------------------
1) Data structs: ModifierDef (Lua), AttachedModifier (runtime instance), ModifierRegistry
2) Scope storage: per-VID lists for guns/items/projectiles; per-player list
3) Stats pipeline: build EffectiveGunParams at fire/reload; EffectivePlayerStats for movement
4) Hook routing: iterate active modifiers across scopes; call Lua with ctx
5) Stack/duration ticking each frame; clean up expired
6) Integrate into stacking rules: items only merge if identical modifier signature

Movement Inaccuracy (planned hook)
----------------------------------
- Expose a modifier that affects movement spread dynamics:
  - Fields: move_spread_inc_mult, move_spread_decay_mult, move_spread_flat_add
  - Applied at player scope to adjust how quickly movement inaccuracy accumulates/decays.
  - Good for perks, items, or temporary buffs/debuffs.

Notes
-----
- Deterministic order: sort modifiers by a stable key (id) before application to ensure reproducibility.
- Performance: cache the last computed EffectiveGunParams until inputs change.
- Save/load: modifier lists must be serializable.
