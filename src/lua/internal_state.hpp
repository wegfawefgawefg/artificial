#pragma once

#include <unordered_map>
#include <sol/sol.hpp>

struct ItemHooks {
    sol::protected_function on_use;
    sol::protected_function on_tick;
    sol::protected_function on_shoot;
    sol::protected_function on_damage;
    sol::protected_function on_active_reload;
    sol::protected_function on_failed_active_reload;
    sol::protected_function on_tried_after_failed_ar;
    sol::protected_function on_pickup;
    sol::protected_function on_drop;
    sol::protected_function on_eject;
    sol::protected_function on_reload_start;
    sol::protected_function on_reload_finish;
};

struct GunHooks {
    sol::protected_function on_jam;
    sol::protected_function on_active_reload;
    sol::protected_function on_failed_active_reload;
    sol::protected_function on_tried_after_failed_ar;
    sol::protected_function on_pickup;
    sol::protected_function on_drop;
    sol::protected_function on_step;
    sol::protected_function on_eject;
    sol::protected_function on_reload_start;
    sol::protected_function on_reload_finish;
};

struct AmmoHooks {
    sol::protected_function on_hit;
    sol::protected_function on_hit_entity;
    sol::protected_function on_hit_tile;
};

struct ProjectileHooks {
    sol::protected_function on_hit_entity;
    sol::protected_function on_hit_tile;
};

struct CrateHooks { sol::protected_function on_open; };

struct EntityHooks {
    sol::protected_function on_step;
    sol::protected_function on_damage;
    sol::protected_function on_spawn;
    sol::protected_function on_death;
    sol::protected_function on_reload_start;
    sol::protected_function on_reload_finish;
    sol::protected_function on_gun_jam;
    sol::protected_function on_out_of_ammo;
    sol::protected_function on_hp_under_50;
    sol::protected_function on_hp_under_25;
    sol::protected_function on_hp_full;
    sol::protected_function on_shield_under_50;
    sol::protected_function on_shield_under_25;
    sol::protected_function on_shield_full;
    sol::protected_function on_plates_lost;
    sol::protected_function on_collide_tile;
};

struct GlobalHooks {
    sol::protected_function on_dash;
    sol::protected_function on_active_reload;
    sol::protected_function on_failed_active_reload;
    sol::protected_function on_tried_after_failed_ar;
    sol::protected_function on_step;
    sol::protected_function on_eject;
    sol::protected_function on_reload_start;
    sol::protected_function on_reload_finish;
};

struct LuaHooks {
    std::unordered_map<int, ItemHooks> items;
    std::unordered_map<int, GunHooks> guns;
    std::unordered_map<int, AmmoHooks> ammo;
    std::unordered_map<int, ProjectileHooks> projectiles;
    std::unordered_map<int, CrateHooks> crates;
    std::unordered_map<int, EntityHooks> entities;
    GlobalHooks global;
};

