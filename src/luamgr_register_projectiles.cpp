#include "luamgr.hpp"
#include "lua/lua_bindings.hpp"

void lua_register_projectiles(sol::state& s, LuaManager& m) {
    s.set_function("register_projectile", [&m](sol::table t) {
        ProjectileDef d{};
        d.name = t.get_or("name", std::string{});
        d.type = t.get_or("type", 0);
        d.speed = t.get_or("speed", 20.0f);
        d.size_x = t.get_or("size_x", 0.2f);
        d.size_y = t.get_or("size_y", 0.2f);
        d.physics_steps = t.get_or("physics_steps", 2);
        d.sprite = t.get_or("sprite", std::string{});
        if (auto o = t.get<sol::object>("on_hit_entity"); o.is<sol::function>()) d.on_hit_entity = o.as<sol::protected_function>();
        if (auto o = t.get<sol::object>("on_hit_tile"); o.is<sol::function>()) d.on_hit_tile = o.as<sol::protected_function>();
        m.add_projectile(d);
    });
}

