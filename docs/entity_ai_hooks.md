Entity AI Hooks (Current + Planned)

Current hooks (Lua register_entity_type)
- on_step: periodic tick; controlled by tick_rate_hz and tick_phase ("before" or "after").
- on_spawn: called when an entity instance is created.
- on_death: called when an NPC’s HP hits 0.
- on_damage(attacker_ap): called when the entity takes damage (after plates/shields application).
- on_reload_start: called when the entity begins a reload.
- on_reload_finish: called when a reload completes.
- on_gun_jam: called when the equipped gun jams.
- on_out_of_ammo: called when attempting to reload/shoot without reserve ammo.
- on_hp_under_50 / on_hp_under_25 / on_hp_full: edge-triggered HP thresholds.
- on_shield_under_50 / on_shield_under_25 / on_shield_full: edge-triggered shield thresholds.
- on_plates_lost: called when plates drop to 0.
- on_collide_tile: called when movement is blocked by an impassable tile.

Notes
- Hooks are no-ops if not provided. Overhead is minimal when undefined.
- HP/shield threshold hooks are edge-triggered to avoid spamming; call when crossing the boundary only.
- on_collide_tile fires on collision resolution per axis; avoid expensive Lua work here.
- on_reload_* and on_gun_jam currently fire for the player. As entity inventories unify and AI uses guns, these hooks will apply to any entity that reloads/jams.

Planned expansions
- on_collide_entity(other): collisions with other entities.
- on_pickup / on_drop: per-entity item pickup/drop callbacks.
- on_equip_gun / on_unequip_gun: equip state changes.
- on_fire / on_hit: entity-scope fire/hit events separate from ammo/projectile hooks.
- Behavior helpers: pathing/steering utilities exposed to Lua for AI movement.

