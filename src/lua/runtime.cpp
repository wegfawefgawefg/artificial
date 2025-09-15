#include "luamgr.hpp"
#include "lua/lua_helpers.hpp"
#include "lua/bindings.hpp"
#include "lua/version.hpp"
#include "globals.hpp"
#include "lua/internal_state.hpp"

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-conversion"
#endif
#include <sol/sol.hpp>
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

#include <cstdio>
#include <filesystem>

namespace fs = std::filesystem;

LuaManager::LuaManager() {}

LuaManager::~LuaManager() {
    // Destroy hooks (which hold sol::protected_function) BEFORE destroying the Lua state.
    if (hooks_) { delete hooks_; hooks_ = nullptr; }
    if (S) { delete S; S = nullptr; L = nullptr; }
}

bool LuaManager::available() const { return S != nullptr; }

void LuaManager::clear() {
    powerups_.clear();
    items_.clear();
    guns_.clear();
    projectiles_.clear();
    ammo_.clear();
    crates_.clear();
    entity_types_.clear();
}

bool LuaManager::init() {
    if (S)
        return true;
    S = new sol::state();
    S->open_libraries(sol::lib::base, sol::lib::math, sol::lib::package, sol::lib::string,
                      sol::lib::table, sol::lib::os);
    L = S->lua_state();
    if (!hooks_) hooks_ = new LuaHooks();
    return register_api();
}

bool LuaManager::register_api() {
    g_mgr = this;
    sol::state& s = *S;

    // Subsystem registrations
    lua_register_powerups(s, *this);
    lua_register_items(s, *this);
    lua_register_guns(s, *this);
    lua_register_ammo(s, *this);
    lua_register_projectiles(s, *this);
    lua_register_crates(s, *this);
    lua_register_entities(s, *this);

    // Named table 'api' functions
    lua_register_api_player(s, *this);
    lua_register_api_world(s, *this);

    // Optional registered global handlers
    s.set_function("register_on_dash", [this](sol::function f) { hooks_->global.on_dash = sol::protected_function(f); });
    s.set_function("register_on_active_reload", [this](sol::function f) { hooks_->global.on_active_reload = sol::protected_function(f); });
    s.set_function("register_on_step", [this](sol::function f) { hooks_->global.on_step = sol::protected_function(f); });
    s.set_function("register_on_failed_active_reload", [this](sol::function f) { hooks_->global.on_failed_active_reload = sol::protected_function(f); });
    s.set_function("register_on_tried_to_active_reload_after_failing", [this](sol::function f) { hooks_->global.on_tried_after_failed_ar = sol::protected_function(f); });
    s.set_function("register_on_eject", [this](sol::function f) { hooks_->global.on_eject = sol::protected_function(f); });
    s.set_function("register_on_reload_start", [this](sol::function f) { hooks_->global.on_reload_start = sol::protected_function(f); });
    s.set_function("register_on_reload_finish", [this](sol::function f) { hooks_->global.on_reload_finish = sol::protected_function(f); });

    return true;
}

bool LuaManager::run_file(const std::string& path) {
    sol::protected_function_result r = S->safe_script_file(path);
    if (!r.valid()) {
        sol::error e = r;
        std::fprintf(stderr, "[lua] error in %s: %s\n", path.c_str(), e.what());
        return false;
    }
    return true;
}

void LuaManager::call_generate_room() {
    sol::object obj = S->get<sol::object>("generate_room");
    if (!obj.is<sol::function>()) return;
    LuaCtxGuard _ctx(ss, nullptr);
    auto r = obj.as<sol::protected_function>()();
    if (!r.valid()) { sol::error e = r; std::fprintf(stderr, "[lua] error in generate_room: %s\n", e.what()); }
}

bool LuaManager::load_mods() {
    auto mods_root = mm->root;
    clear();
    std::error_code ec;
    if (!fs::exists(mods_root, ec) || !fs::is_directory(mods_root, ec))
        return false;
    for (auto const& mod : fs::directory_iterator(mods_root, ec)) {
        if (ec) { ec.clear(); continue; }
        if (!mod.is_directory()) continue;
        fs::path sdir = mod.path() / "scripts";
        if (fs::exists(sdir, ec) && fs::is_directory(sdir, ec)) {
            // Clear per-mod api_version global before loading this mod's scripts
            S->set("api_version", sol::lua_nil);
            for (auto const& f : fs::directory_iterator(sdir, ec)) {
                if (ec) { ec.clear(); continue; }
                if (!f.is_regular_file()) continue;
                auto p = f.path();
                if (p.extension() == ".lua")
                    run_file(p.string());
            }
            // Per-mod api_version check
            sol::object vobj = S->get<sol::object>("api_version");
            if (vobj.is<sol::table>()) {
                sol::table vt = vobj;
                int major = vt.get_or("major", 0);
                int minor = vt.get_or("minor", 0);
                if (major != lua_api::kApiMajor) {
                    std::fprintf(stderr, "[lua] [%s] api_version mismatch: mod=%d.%d engine=%d.%d (major must match)\n",
                                 mod.path().filename().string().c_str(), major, minor, lua_api::kApiMajor, lua_api::kApiMinor);
                } else if (minor > lua_api::kApiMinor) {
                    std::fprintf(stderr, "[lua] [%s] api_version minor too new: mod=%d.%d engine=%d.%d (features may be missing)\n",
                                 mod.path().filename().string().c_str(), major, minor, lua_api::kApiMajor, lua_api::kApiMinor);
                }
            }
            // Clear to avoid leaking into next mod's check
            S->set("api_version", sol::lua_nil);
        }
    }
    std::printf("[lua] loaded: %zu powerups, %zu items, %zu guns, %zu ammo, %zu projectiles, %zu entity types\n",
                powerups_.size(), items_.size(), guns_.size(), ammo_.size(), projectiles_.size(), entity_types_.size());
    // Optional drop tables
    drops_.powerups.clear();
    drops_.items.clear();
    drops_.guns.clear();
    if (S) {
        sol::object dobj = S->get<sol::object>("drops");
        if (dobj.is<sol::table>()) {
            sol::table dt = dobj;
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
            parse_list("powerups", drops_.powerups);
            parse_list("items", drops_.items);
            parse_list("guns", drops_.guns);
        }
    }
    // Note: per-mod api_version check performed during load above.
    return true;
}
