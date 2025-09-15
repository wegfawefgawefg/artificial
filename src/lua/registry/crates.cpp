#include "luamgr.hpp"
#include "lua/bindings.hpp"
#include "lua/internal_state.hpp"

void lua_register_crates(sol::state& s, LuaManager& m) {
    s.set_function("register_crate", [&m](sol::table t) {
        CrateDef d{};
        d.name = t.get_or("name", std::string{});
        d.type = t.get_or("type", 0);
        d.open_time = t.get_or("open_time", 5.0f);
        d.label = t.get_or("label", std::string{});
        // optional on_open stored internally
        sol::object onopen = t.get<sol::object>("on_open");
        CrateHooks ch{}; if (onopen.is<sol::function>()) ch.on_open = onopen.as<sol::protected_function>();
        // optional drops table
        sol::object dopts = t.get<sol::object>("drops");
        if (dopts.is<sol::table>()) {
            sol::table dt = dopts;
            auto parse_list = [&](const char* key, std::vector<DropEntry>& out) {
                sol::object arr = dt.get<sol::object>(key);
                if (!arr.is<sol::table>()) return;
                sol::table tarr = arr;
                for (auto& kv : tarr) {
                    sol::object v = kv.second;
                    if (v.is<sol::table>()) {
                        sol::table e = v;
                        DropEntry de{};
                        de.type = e.get_or("type", 0);
                        de.weight = e.get_or("weight", 1.0f);
                        out.push_back(de);
                    }
                }
            };
            parse_list("powerups", d.drops.powerups);
            parse_list("items", d.drops.items);
            parse_list("guns", d.drops.guns);
        }
        m.add_crate(d);
        if (d.type != 0 && m.hooks_) m.hooks_->crates[d.type] = ch;
    });
}
