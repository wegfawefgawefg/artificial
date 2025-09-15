#include "luamgr.hpp"
#include "lua/bindings.hpp"
#include "lua/internal_state.hpp"

void lua_register_powerups(sol::state& s, LuaManager& m) {
    s.set_function("register_powerup", [&m](sol::table t) {
        PowerupDef d{};
        d.name = t.get_or("name", std::string{});
        d.type = t.get_or("type", 0);
        d.sprite = t.get_or("sprite", std::string{});
        m.add_powerup(d);
    });
}

void lua_register_items(sol::state& s, LuaManager& m) {
    s.set_function("register_item", [&m](sol::table t) {
        ItemDef d{};
        d.name = t.get_or("name", std::string{});
        d.type = t.get_or("type", 0);
        d.category = t.get_or("category", 0);
        d.max_count = t.get_or("max_count", 1);
        d.consume_on_use = t.get_or("consume_on_use", false);
        d.sprite = t.get_or("sprite", std::string{});
        d.desc = t.get_or("desc", std::string{});
        d.sound_use = t.get_or("sound_use", std::string{});
        d.sound_pickup = t.get_or("sound_pickup", std::string{});
        ItemHooks ih{};
        if (auto o = t.get<sol::object>("on_use"); o.is<sol::function>()) ih.on_use = o.as<sol::protected_function>();
        d.tick_rate_hz = t.get_or("tick_rate_hz", 0.0f);
        d.tick_phase = t.get_or("tick_phase", std::string("after"));
        if (auto o = t.get<sol::object>("on_active_reload"); o.is<sol::function>()) ih.on_active_reload = o.as<sol::protected_function>();
        if (auto o = t.get<sol::object>("on_failed_active_reload"); o.is<sol::function>()) ih.on_failed_active_reload = o.as<sol::protected_function>();
        if (auto o = t.get<sol::object>("on_tried_to_active_reload_after_failing"); o.is<sol::function>()) ih.on_tried_after_failed_ar = o.as<sol::protected_function>();
        if (auto o = t.get<sol::object>("on_pickup"); o.is<sol::function>()) ih.on_pickup = o.as<sol::protected_function>();
        if (auto o = t.get<sol::object>("on_drop"); o.is<sol::function>()) ih.on_drop = o.as<sol::protected_function>();
        m.add_item(d);
        if (d.type != 0 && m.hooks_) m.hooks_->items[d.type] = ih;
    });
}
