Per-Entity Interface (Design Notes)

Goal
- Every entity (player, NPC, anything) shares one flat, capability-complete interface: inventory, weapons/items usage, stats, damage model, hooks.
- Player-specific “classes” sit on top (as loadouts/modifier sets), not as a unique hardcoded entity type.

Current status
- Stats in C++ entity model already mirror the SYNTHETIK baseline: max_health, health_regen, shield_max, shield_regen, armor, move_speed, dodge, scavenging, currency, ammo_gain, luck, crit_chance, crit_damage, headshot_damage, damage_absorb, damage_output, healing, accuracy, terror_level, plus armor plates and movement spread dynamics.
- Lua entity types: currently expose a useful subset (sizes, hp/shield/regen/armor/plates, move_speed, accuracy, movement-spread). Hooks available for spawn/death/damage, gun/reload, thresholds, and collisions. Periodic on_step supports before/after phases.
- Inventories: per-entity inventories exist (pooled by VID). Player UI and gameplay now use the player’s per-entity inventory.
- Guns/Items ticks: both before and after phases wired and running against the player’s per-entity inventory.
- Hot reload: changing Lua updates live entities’ sprite and collider sizes.

Open tasks
- Expand Lua entity defs to cover the full stats set (dodge, scavenging, currency, ammo_gain, luck, crit stats, headshot, damage_absorb, damage_output, healing, terror_level, health_regen), with sensible defaults.
- Make all weapon actions (shoot, reload, jam, ammo handling) operate for any entity that equips a gun, not only the player paths.
- Per-entity pickup/drop/use for items and guns, with optional on_pickup/on_drop entity hooks.
- Player classes: reserve a concept of “player class” as a selectable build (mods define classes with modifiers/loadouts). Player remains a generic entity, class augments it at spawn.
  - Note: Classes are effectively entity presets (stats + loadouts + modifiers). Players can swap equipment later; class is a starting configuration and passive modifier set, not a hard-typed entity.
- Optimize lookups: change entity type lookup from vector scan to a map for lower hook-call overhead if needed.

Performance note
- All hooks are guarded; if a Lua function isn’t supplied, we do not enter Lua. Overhead stays at a few comparisons and (currently) a small vector scan for the entity type def. Threshold hooks fire only on boundary crossings.
