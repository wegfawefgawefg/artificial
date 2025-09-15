#include "luamgr.hpp"
#include "lua/bindings.hpp"
#include "lua/internal_state.hpp"

void lua_register_entities(sol::state& s, LuaManager& m) {
    s.set_function("register_entity_type", [&m](sol::table t) {
        EntityTypeDef d{};
        d.name = t.get_or("name", std::string{});
        d.type = t.get_or("type", 0);
        d.sprite = t.get_or("sprite", std::string{});
        d.sprite_w = t.get_or("sprite_w", 0.25f);
        d.sprite_h = t.get_or("sprite_h", 0.25f);
        d.collider_w = t.get_or("collider_w", 0.25f);
        d.collider_h = t.get_or("collider_h", 0.25f);
        d.physics_steps = t.get_or("physics_steps", 1);
        {
            int mhp = t.get_or("max_hp", 1000);
            if (mhp < 0) mhp = 0;
            d.max_hp = static_cast<uint32_t>(mhp);
        }
        d.shield_max = t.get_or("shield_max", 0.0f);
        d.shield_regen = t.get_or("shield_regen", 0.0f);
        d.health_regen = t.get_or("health_regen", 0.0f);
        d.armor = t.get_or("armor", 0.0f);
        d.plates = t.get_or("plates", 0);
        d.move_speed = t.get_or("move_speed", 350.0f);
        d.dodge = t.get_or("dodge", 3.0f);
        d.accuracy = t.get_or("accuracy", 100.0f);
        d.scavenging = t.get_or("scavenging", 100.0f);
        d.currency = t.get_or("currency", 100.0f);
        d.ammo_gain = t.get_or("ammo_gain", 100.0f);
        d.luck = t.get_or("luck", 100.0f);
        d.crit_chance = t.get_or("crit_chance", 3.0f);
        d.crit_damage = t.get_or("crit_damage", 200.0f);
        d.headshot_damage = t.get_or("headshot_damage", 200.0f);
        d.damage_absorb = t.get_or("damage_absorb", 100.0f);
        d.damage_output = t.get_or("damage_output", 100.0f);
        d.healing = t.get_or("healing", 100.0f);
        d.terror_level = t.get_or("terror_level", 100.0f);
        d.move_spread_inc_rate_deg_per_sec_at_base = t.get_or("move_spread_inc_rate_deg_per_sec_at_base", 8.0f);
        d.move_spread_decay_deg_per_sec = t.get_or("move_spread_decay_deg_per_sec", 10.0f);
        d.move_spread_max_deg = t.get_or("move_spread_max_deg", 20.0f);
        d.tick_rate_hz = t.get_or("tick_rate_hz", 0.0f);
        d.tick_phase = t.get_or("tick_phase", std::string("after"));
        EntityHooks eh{};
        if (auto o = t.get<sol::object>("on_step"); o.is<sol::function>()) eh.on_step = o.as<sol::protected_function>();
        if (auto o = t.get<sol::object>("on_damage"); o.is<sol::function>()) eh.on_damage = o.as<sol::protected_function>();
        if (auto o = t.get<sol::object>("on_spawn"); o.is<sol::function>()) eh.on_spawn = o.as<sol::protected_function>();
        if (auto o = t.get<sol::object>("on_death"); o.is<sol::function>()) eh.on_death = o.as<sol::protected_function>();
        if (auto o = t.get<sol::object>("on_reload_start"); o.is<sol::function>()) eh.on_reload_start = o.as<sol::protected_function>();
        if (auto o = t.get<sol::object>("on_reload_finish"); o.is<sol::function>()) eh.on_reload_finish = o.as<sol::protected_function>();
        if (auto o = t.get<sol::object>("on_gun_jam"); o.is<sol::function>()) eh.on_gun_jam = o.as<sol::protected_function>();
        if (auto o = t.get<sol::object>("on_out_of_ammo"); o.is<sol::function>()) eh.on_out_of_ammo = o.as<sol::protected_function>();
        if (auto o = t.get<sol::object>("on_hp_under_50"); o.is<sol::function>()) eh.on_hp_under_50 = o.as<sol::protected_function>();
        if (auto o = t.get<sol::object>("on_hp_under_25"); o.is<sol::function>()) eh.on_hp_under_25 = o.as<sol::protected_function>();
        if (auto o = t.get<sol::object>("on_hp_full"); o.is<sol::function>()) eh.on_hp_full = o.as<sol::protected_function>();
        if (auto o = t.get<sol::object>("on_shield_under_50"); o.is<sol::function>()) eh.on_shield_under_50 = o.as<sol::protected_function>();
        if (auto o = t.get<sol::object>("on_shield_under_25"); o.is<sol::function>()) eh.on_shield_under_25 = o.as<sol::protected_function>();
        if (auto o = t.get<sol::object>("on_shield_full"); o.is<sol::function>()) eh.on_shield_full = o.as<sol::protected_function>();
        if (auto o = t.get<sol::object>("on_plates_lost"); o.is<sol::function>()) eh.on_plates_lost = o.as<sol::protected_function>();
        if (auto o = t.get<sol::object>("on_collide_tile"); o.is<sol::function>()) eh.on_collide_tile = o.as<sol::protected_function>();
        m.add_entity_type(d);
        if (d.type != 0 && m.hooks_) m.hooks_->entities[d.type] = eh;
    });
}
