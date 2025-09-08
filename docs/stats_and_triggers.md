# Stats, Items, and Triggers (Lua)

## Player/Entity Stats (subset)
- max_health, health_regen
- shield_max, shield_regen
- armor (damage reduction %), plates (one-hit immunity count)
- move_speed, dodge, scavenging, currency, ammo_gain, luck
- crit_chance, crit_damage, headshot_damage
- damage_absorb, damage_output, healing, accuracy, terror_level

## Items (Lua)
Use `register_item{ ... }` to define items.

Fields:
- name: string
- type: int (unique id)
- category: int (1=usable, 2=gun, 0=other)
- max_count: int (stack size)
- consume_on_use: bool
- sprite: string (sprite key)
- desc: string (short description for UI)
- on_use(): optional function, called when the item is used
- on_tick(dt): optional function, called every fixed step while the item is in inventory
- on_shoot(): optional function, called when the player fires
- on_damage(attacker_ap): optional function, called when the owner takes damage (hooked up as needed)

## API Functions (Lua)
- api.heal(n): heals the player by n (clamped to max)
- api.add_plate(n): adds n armor plates (one-hit immunities)
- api.add_move_speed(n): adds n to move_speed (temporary/simple)

## Triggers Call Order
- on_tick(dt): every fixed update (144 Hz) for items in the player inventory.
- on_use(): when the player uses an item in a slot.
- on_shoot(): when the player fires.
- on_damage(attacker_ap): reserved for when the owner takes damage.

## Notes
- Use `desc` for display text; on_use should perform actions, not return strings.
- Sprites are loaded by name; place PNGs under mods/<mod>/graphics and use the filename stem as the sprite key.
- Guns are registered with `register_gun{ name, type, damage, rpm, recoil, control, mag, ammo_max }` and live instances are managed by the engine.
