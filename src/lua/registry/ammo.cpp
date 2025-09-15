#include "luamgr.hpp"
#include "lua/bindings.hpp"
#include "lua/internal_state.hpp"

void lua_register_ammo(sol::state& s, LuaManager& m) {
    s.set_function("register_ammo", [&m](sol::table t) {
        AmmoDef d{};
        d.name = t.get_or("name", std::string{});
        d.type = t.get_or("type", 0);
        d.desc = t.get_or("desc", std::string{});
        d.sprite = t.get_or("sprite", std::string{});
        d.size_x = t.get_or("size_x", 0.2f);
        d.size_y = t.get_or("size_y", 0.2f);
        d.speed = t.get_or("speed", 20.0f);
        d.damage_mult = t.get_or("damage_mult", 1.0f);
        d.armor_pen = t.get_or("armor_pen", 0.0f);
        d.shield_mult = t.get_or("shield_mult", 1.0f);
        d.range_units = t.get_or("range", 0.0f);
        d.falloff_start = t.get_or("falloff_start", 0.0f);
        d.falloff_end = t.get_or("falloff_end", 0.0f);
        d.falloff_min_mult = t.get_or("falloff_min_mult", 1.0f);
        d.pierce_count = t.get_or("pierce_count", 0);
        // Optional hooks stored internally
        AmmoHooks ah{};
        if (auto o = t.get<sol::object>("on_hit"); o.is<sol::function>()) ah.on_hit = o.as<sol::protected_function>();
        if (auto o = t.get<sol::object>("on_hit_entity"); o.is<sol::function>()) ah.on_hit_entity = o.as<sol::protected_function>();
        if (auto o = t.get<sol::object>("on_hit_tile"); o.is<sol::function>()) ah.on_hit_tile = o.as<sol::protected_function>();
        m.add_ammo(d);
        if (d.type != 0 && m.hooks_) m.hooks_->ammo[d.type] = ah;
    });
}
