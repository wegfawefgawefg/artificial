#include "luamgr.hpp"
#include "lua/bindings.hpp"
#include "lua/internal_state.hpp"

void lua_register_guns(sol::state& s, LuaManager& m) {
    s.set_function("register_gun", [&m](sol::table t) {
        GunDef d{};
        d.name = t.get_or("name", std::string{});
        d.type = t.get_or("type", 0);
        d.damage = t.get_or("damage", 0.0f);
        d.rpm = t.get_or("rpm", 0.0f);
        d.deviation = t.get_or("deviation", 0.0f);
        d.recoil = t.get_or("recoil", 0.0f);
        d.control = t.get_or("control", 0.0f);
        d.max_recoil_spread_deg = t.get_or("max_recoil_spread_deg", 12.0f);
        d.pellets_per_shot = t.get_or("pellets", 1);
        d.mag = t.get_or("mag", 0);
        d.ammo_max = t.get_or("ammo_max", 0);
        d.sprite = t.get_or("sprite", std::string{});
        d.jam_chance = t.get_or("jam_chance", 0.0f);
        d.projectile_type = t.get_or("projectile_type", 0);
        d.sound_fire = t.get_or("sound_fire", std::string{});
        d.sound_reload = t.get_or("sound_reload", std::string{});
        d.sound_jam = t.get_or("sound_jam", std::string{});
        d.sound_pickup = t.get_or("sound_pickup", std::string{});
        d.tick_rate_hz = t.get_or("tick_rate_hz", 0.0f);
        d.tick_phase = t.get_or("tick_phase", std::string("after"));
        d.fire_mode = t.get_or("fire_mode", std::string("auto"));
        d.burst_count = t.get_or("burst_count", 0);
        d.burst_rpm = t.get_or("burst_rpm", 0.0f);
        d.shot_interval = t.get_or("shot_interval", 0.0f);
        d.burst_interval = t.get_or("burst_interval", 0.0f);
        d.reload_time = t.get_or("reload_time", 1.0f);
        d.eject_time = t.get_or("eject_time", 0.2f);
        // Active reload fields with legacy fallback
        d.active_reload_window = t.get_or("active_reload_window", 0.0f);
        d.ar_size = t.get_or("ar_size", d.active_reload_window > 0.0f ? d.active_reload_window : 0.15f);
        d.ar_size_variance = t.get_or("ar_size_variance", 0.0f);
        d.ar_pos = t.get_or("ar_pos", 0.5f);
        // Additional gun reload hooks (stored internally)
        GunHooks gh{};
        if (auto o = t.get<sol::object>("on_eject"); o.is<sol::function>()) gh.on_eject = o.as<sol::protected_function>();
        sol::object orls = t.get<sol::object>("on_reload_start");
        if (orls.is<sol::function>()) gh.on_reload_start = orls.as<sol::protected_function>();
        sol::object orlf = t.get<sol::object>("on_reload_finish");
        if (orlf.is<sol::function>()) gh.on_reload_finish = orlf.as<sol::protected_function>();
        d.ar_pos_variance = t.get_or("ar_pos_variance", 0.0f);
        if (auto o = t.get<sol::object>("on_jam"); o.is<sol::function>()) gh.on_jam = o.as<sol::protected_function>();
        if (auto o = t.get<sol::object>("on_active_reload"); o.is<sol::function>()) gh.on_active_reload = o.as<sol::protected_function>();
        if (auto o = t.get<sol::object>("on_failed_active_reload"); o.is<sol::function>()) gh.on_failed_active_reload = o.as<sol::protected_function>();
        if (auto o = t.get<sol::object>("on_tried_to_active_reload_after_failing"); o.is<sol::function>()) gh.on_tried_after_failed_ar = o.as<sol::protected_function>();
        if (auto o = t.get<sol::object>("on_pickup"); o.is<sol::function>()) gh.on_pickup = o.as<sol::protected_function>();
        if (auto o = t.get<sol::object>("on_drop"); o.is<sol::function>()) gh.on_drop = o.as<sol::protected_function>();
        if (auto o = t.get<sol::object>("on_step"); o.is<sol::function>()) gh.on_step = o.as<sol::protected_function>();
        // Optional compatible_ammo list: { {type=..., weight=...}, ... }
        sol::object ammo_list_obj = t.get<sol::object>("compatible_ammo");
        if (ammo_list_obj.is<sol::table>()) {
            sol::table arr = ammo_list_obj;
            for (auto& kv : arr) {
                sol::object v = kv.second;
                if (v.is<sol::table>()) {
                    sol::table e = v;
                    AmmoCompat ac{};
                    ac.type = e.get_or("type", 0);
                    ac.weight = e.get_or("weight", 1.0f);
                    if (ac.type != 0 && ac.weight > 0.0f)
                        d.compatible_ammo.push_back(ac);
                }
            }
        }
        m.add_gun(d);
        if (d.type != 0 && m.hooks_) m.hooks_->guns[d.type] = gh;
    });
}
