-- Base content defined via Lua

-- Sprites will be registered via data files/images elsewhere; here we register items and guns

register_powerup{ name = "Power Core", type = 1, sprite = "base:power_core" }
register_powerup{ name = "Stim Pack", type = 2, sprite = "base:stim_pack" }

register_item{ name = "Bandage", type = 100, category = 1, max_count = 5, consume_on_use = true, sprite = "base:bandage", desc = "+25 HP",
  on_use = function() api.heal(25) end }
register_item{ name = "Health Kit", type = 101, category = 1, max_count = 3, consume_on_use = true, sprite = "base:medkit", desc = "+100 HP",
  on_use = function() api.heal(100) end, sound_use = "base:ui_confirm" }
register_item{ name = "Conductor Hat", type = 102, category = 1, max_count = 1, consume_on_use = false, sprite = "base:conductor_hat", desc = "+50 move speed",
  on_use = function() api.add_move_speed(50) end }
register_item{ name = "Armor Plate", type = 103, category = 1, max_count = 5, consume_on_use = true, sprite = "base:armor_plate", desc = "+1 Plate",
  on_use = function() api.add_plate(1) end }

-- Simple bullet projectile def
register_projectile{ name = "base_bullet", type = 1, speed = 28, size_x = 0.12, size_y = 0.12, physics_steps = 2,
  on_hit_entity = function() end,
  on_hit_tile = function() end }

register_gun{ name = "Pistol", type = 200, damage = 20, rpm = 350, recoil = 0.5, control = 0.8, mag = 12, ammo_max = 180, sprite="base:pistol", jam_chance=0.01, projectile_type=1,
  fire_mode="single", sound_fire="base:small_shoot", sound_reload="base:reload", sound_jam="base:ui_cant", sound_pickup="base:drop" }
register_gun{ name = "Rifle", type = 201, damage = 15, rpm = 650, recoil = 0.9, control = 0.7, mag = 30, ammo_max = 240, sprite="base:rifle", jam_chance=0.005, projectile_type=1,
  fire_mode="auto", sound_fire="base:medium_shoot", sound_reload="base:reload", sound_jam="base:ui_cant", sound_pickup="base:drop" }

-- Semi-auto 5-shot burst variant for testing
register_gun{ name = "Burst Rifle", type = 202, damage = 15, rpm = 600, recoil = 0.9, control = 0.7, mag = 30, ammo_max = 240, sprite="base:rifle", jam_chance=0.005, projectile_type=1,
  fire_mode="burst", burst_count=5, burst_rpm=1100, shot_interval=0.10, burst_interval=0.0545,
  sound_fire="base:medium_shoot", sound_reload="base:reload", sound_jam="base:ui_cant", sound_pickup="base:drop" }

-- Very inaccurate shotgun-esque test (uses burst to simulate pellets quickly)
register_gun{ name = "Shotgun", type = 203, damage = 6, rpm = 80, recoil = 2.0, control = 6.0, deviation = 8.0,
  mag = 6, ammo_max = 48, sprite="base:rifle", jam_chance=0.003, projectile_type=1,
  fire_mode="single", pellets=8,
  sound_fire="base:medium_shoot", sound_reload="base:reload", sound_jam="base:ui_cant", sound_pickup="base:drop" }

-- Pump-2 (two-shot), Semi-auto, and Full-auto shotguns
register_gun{ name = "Pump-2", type = 210, damage = 7, rpm = 55, recoil = 4.0, control = 7.0, deviation = 7.5,
  mag = 2, ammo_max = 40, sprite="base:rifle", jam_chance=0.004, projectile_type=1,
  fire_mode="single", pellets=8,
  sound_fire="base:medium_shoot", sound_reload="base:reload" }

register_gun{ name = "Semi-Auto SG", type = 211, damage = 6, rpm = 140, recoil = 3.0, control = 6.0, deviation = 7.0,
  mag = 5, ammo_max = 50, sprite="base:rifle", jam_chance=0.004, projectile_type=1,
  fire_mode="single", pellets=8,
  sound_fire="base:medium_shoot", sound_reload="base:reload" }

register_gun{ name = "Full-Auto SG", type = 212, damage = 5, rpm = 300, recoil = 2.2, control = 5.5, deviation = 8.5,
  mag = 20, ammo_max = 120, sprite="base:rifle", jam_chance=0.004, projectile_type=1,
  fire_mode="auto", pellets=6,
  sound_fire="base:medium_shoot", sound_reload="base:reload" }

-- Optional drop tables
drops = {
  powerups = {
    { type = 1, weight = 1.0 },
    { type = 2, weight = 0.8 },
  },
  items = {
    { type = 100, weight = 1.0 },
    { type = 101, weight = 0.8 },
    { type = 103, weight = 0.6 },
  },
  guns = {
    { type = 200, weight = 1.0 },
    { type = 201, weight = 0.7 },
    { type = 202, weight = 0.6 },
    { type = 203, weight = 0.5 },
    { type = 210, weight = 0.4 },
    { type = 211, weight = 0.3 },
    { type = 212, weight = 0.2 },
  }
}

-- Crates
register_crate{ name="Med Crate", type=1, label="Med", open_time=5.0,
  drops = { items = { {type=100, weight=1.0}, {type=101, weight=0.5} } },
  on_open = function() api.heal(100) end }

register_crate{ name="Ammo Crate", type=2, label="Ammo", open_time=5.0,
  drops = { guns = { {type=200, weight=1.0}, {type=201, weight=0.6} } },
  on_open = function() api.refill_ammo() end }

-- Optional: let Lua generate the room content
function generate_room()
  api.spawn_crate_safe(1, 2.5, 0.5)
  api.spawn_crate_safe(2, 0.5, 2.5)
end
