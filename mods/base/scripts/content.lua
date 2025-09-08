-- Base content defined via Lua

-- Sprites will be registered via data files/images elsewhere; here we register items and guns

register_powerup{ name = "Power Core", type = 1, sprite = "power_core" }
register_powerup{ name = "Stim Pack", type = 2, sprite = "stim_pack" }

register_item{
  name = "Bandage", type = 100, category = 1, max_count = 5, consume_on_use = true, sprite = "bandage",
  on_use = function()
    api.heal(25)
    return "Bandage: +25 HP"
  end
}
register_item{
  name = "Health Kit", type = 101, category = 1, max_count = 3, consume_on_use = true, sprite = "health_kit",
  on_use = function()
    api.heal(100)
    return "Health Kit: +100 HP"
  end
}
register_item{
  name = "Conductor Hat", type = 102, category = 1, max_count = 1, consume_on_use = false, sprite = "conductor_hat",
  on_use = function()
    api.add_move_speed(50)
    return "Conductor Hat: +50 move speed"
  end
}
register_item{
  name = "Armor Plate", type = 103, category = 1, max_count = 5, consume_on_use = true, sprite = "armor_plate",
  on_use = function()
    api.add_plate(1)
    return "+1 Armor Plate"
  end
}

register_gun{ name = "Pistol", type = 200, damage = 20, rpm = 350, recoil = 0.5, control = 0.8, mag = 12, ammo_max = 180 }
register_gun{ name = "Rifle", type = 201, damage = 15, rpm = 650, recoil = 0.9, control = 0.7, mag = 30, ammo_max = 240 }
