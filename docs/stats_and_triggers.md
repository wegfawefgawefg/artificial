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
- on_tick(dt): optional function. Only runs when opt-in ticking is enabled via `tick_rate_hz`.
- on_pickup(): optional function, called when the item is added to inventory from the ground
- on_drop(): optional function, called when the item is dropped to the ground
- on_shoot(): optional function, called when the player fires
- on_damage(attacker_ap): optional function, called when the owner takes damage (hooked up as needed)
- on_active_reload(): optional function, called on successful active reload
- on_failed_active_reload(): optional function, called when an active reload attempt misses the active window (locks out further attempts this reload)
- on_tried_to_active_reload_after_failing(): optional function, called when player presses reload again during the same reload after failing the active window
- on_eject(): optional function, called at the start of reload (eject phase)
- on_reload_start(): optional function, called when reload begins (after eject setup)
- on_reload_finish(): optional function, called when reload completes normally (not on active reload)

## API Functions (Lua)
- api.heal(n): heals the player by n (clamped to max)
- api.add_plate(n): adds n armor plates (one-hit immunities)
- api.add_move_speed(n): adds n to move_speed (temporary/simple)
- api.set_dash_max(n): sets maximum dash stocks
- api.set_dash_stocks(n): sets current dash stocks (clamped 0..max)
- api.add_dash_stocks(n): adds/removes dash stocks (clamped)

## Triggers Call Order
- on_tick(dt): every fixed update (144 Hz) for items in the player inventory.
- on_use(): when the player uses an item in a slot.
- on_shoot(): when the player fires.
- on_damage(attacker_ap): reserved for when the owner takes damage.
- on_eject(): at beginning of reload (eject phase)
- on_reload_start(): when reload progress starts filling
- on_active_reload(): on successful active reload (instant completion)
- on_reload_finish(): when reload progress completes normally

## Notes
- Use `desc` for display text; on_use should perform actions, not return strings.
- Sprites are loaded by name; place PNGs under mods/<mod>/graphics and use the filename stem as the sprite key.
- Guns are registered with `register_gun{ name, type, damage, rpm, recoil, control, mag, ammo_max }` and live instances are managed by the engine.

## Guns (Lua)
Use `register_gun{ ... }` to define guns.

Additional fields for reload/active reload:
- reload_time (seconds): time to fill the reload bar after eject.
- eject_time (seconds): time spent in eject phase before the bar fills.
- ar_pos (0..1): baseline center position of active reload window.
- ar_pos_variance: random shift (±) applied to `ar_pos` each reload.
- ar_size (0..1): baseline size of the active window.
- ar_size_variance: random size (±) applied each reload.
- on_active_reload(): optional callback for active reload success.
- on_pickup(): optional callback when a ground gun is picked up into inventory
- on_drop(): optional callback when a gun is dropped to the ground
- on_step(): optional callback. Only runs when opt-in ticking is enabled via `tick_rate_hz`.
- on_failed_active_reload(): optional callback for failing the active window.
  - on_tried_to_active_reload_after_failing(): optional callback when pressing reload again after failing during the same reload.
  - on_eject(), on_reload_start(), on_reload_finish(): optional lifecycle callbacks.

Cadence fields (optional):
- shot_interval (seconds): explicit time between normal shots. If omitted/0, UI derives from `rpm`.
- burst_interval (seconds): explicit time between shots inside a burst. If omitted/0, UI derives from `burst_rpm`.

Global hooks (Sol2):
- register_on_dash(function), on_dash()
- register_on_active_reload(function), on_active_reload()
- register_on_step(function), on_step()  // called every fixed simulation step
- register_on_failed_active_reload(function), on_failed_active_reload()
- register_on_tried_to_active_reload_after_failing(function), on_tried_to_active_reload_after_failing()
- register_on_eject(function), on_eject()
- register_on_reload_start(function), on_reload_start()
  - register_on_reload_finish(function), on_reload_finish()
Ticking (opt-in):
- tick_rate_hz (number): frequency for item/gun ticks. If 0 or omitted, no ticking occurs.
- tick_phase ("before"|"after"): phase to call ticks relative to physics. Defaults to "after".
  - before: good for forces/steering to affect this frame’s integration
  - after: good for effects that shouldn’t retro-affect motion

Scheduler guarantees:
- Only hosts with ticking instances are iterated.
- Per-frame tick call cap (e.g., 2–5k); excess spills to next frame via accumulator.
  - Encourage timers/phases/conditions instead of ticks.

## Ammo (Lua)
- Define shared ammo types once and reuse across guns. Ammo influences projectile visuals and damage behavior.

`register_ammo{ ... }` fields:
- name (string), type (int), desc (string)
- sprite (string), size_x (number), size_y (number), speed (number)
- damage_mult (number): scales the gun’s base damage
- armor_pen (0..1): fraction of armor ignored (entity-only)
- shield_mult (number): multiplier applied to shield damage
- range (number): max travel distance in world units (0 = unlimited)
- falloff_start (number), falloff_end (number): distance window for linear falloff
- falloff_min_mult (number): damage multiplier at/after `falloff_end`
- pierce_count (int): number of entities this projectile can pass through before stopping
- on_hit(), on_hit_entity(), on_hit_tile(): optional hooks fired when ammo-bearing projectile hits

Gun ↔ Ammo compatibility:
- Guns may declare `compatible_ammo = { {type=300, weight=0.8}, ... }`.
- A gun’s ammo is chosen on gun spawn via weighted selection; scripts can override.

Runtime ammo swap (Lua API):
- `api.set_equipped_ammo(type)`: sets if compatible with the gun; brief UI alert.
- `api.set_equipped_ammo_force(type)`: sets regardless of compatibility; brief UI alert.
