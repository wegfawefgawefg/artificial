Mechanics Overview
==================

Accuracy & Reticle
------------------
- Total spread (degrees): theta = clamp(min_deg, base_dev/acc + move_spread/acc + recoil_spread, max_deg)
  - base_dev: per-gun `deviation` (deg)
  - acc: per-entity `accuracy` factor (A/100, clamped ≥ 0.1)
  - move_spread: per-entity accumulator (deg) that rises while moving and decays at rest
  - recoil_spread: per-gun accumulator (deg) that rises per shot and decays by control
- Reticle circle: exact visualization at cursor distance
  - r_world = distance(player→cursor) * tan(theta)
  - r_screen = r_world * TILE_SIZE * zoom (clamped to a small minimum for visibility)
- Shots: sample a uniform angle in [-theta, +theta] and rotate aim by that angle before spawning

Recoil
------
- On shot: recoil_spread += `recoil` (deg)
- Recovery: recoil_spread -= `control` (deg/sec), clamped to ≥ 0
- Cap: recoil_spread ≤ `max_recoil_spread_deg` (per-gun)

Movement Inaccuracy
-------------------
- Per-entity accumulator `move_spread_deg` (deg)
  - Increases while moving: + move_spread_inc_rate_deg_per_sec_at_base * (|vel| / PLAYER_SPEED_UNITS_PER_SEC)
  - Decays while still: - move_spread_decay_deg_per_sec
  - Clamped to [0, move_spread_max_deg]
- Dashing contributes via velocity (higher |vel|)

Pellets / Shotguns
------------------
- Per-shot pellets: `pellets` (per-gun) spawns N projectiles with their own angular offsets in the same shot
- Ammo cost: one ammo per trigger; recoil applies once per shot

Linear Dynamics
---------------
- Both recoil and movement spread use linear increase/recovery (deg/sec), making tuning predictable

