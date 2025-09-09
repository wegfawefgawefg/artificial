#include "luamgr.hpp"

#include "entity.hpp"
#include "state.hpp"
#ifdef GUB_USE_SOL2
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-conversion"
#endif
#include <sol/sol.hpp>
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
#endif
#include <cstdio>
#include <filesystem>

#ifdef GUB_ENABLE_LUA
extern "C" {
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
}
#endif

namespace fs = std::filesystem;

LuaManager::LuaManager() {
}
LuaManager::~LuaManager() {
#ifdef GUB_ENABLE_LUA
#ifdef GUB_USE_SOL2
    if (S) {
        delete S;
        S = nullptr;
        L = nullptr;
    }
#else
    if (L) {
        lua_close(L);
        L = nullptr;
    }
#endif
#endif
}

bool LuaManager::available() const {
#ifdef GUB_ENABLE_LUA
    return true;
#else
    return false;
#endif
}

void LuaManager::clear() {
    powerups_.clear();
    items_.clear();
    guns_.clear();
}

bool LuaManager::init() {
#ifdef GUB_ENABLE_LUA
    if (L)
        return true;
#ifdef GUB_USE_SOL2
    S = new sol::state();
    S->open_libraries(sol::lib::base, sol::lib::math, sol::lib::package, sol::lib::string,
                      sol::lib::table, sol::lib::os);
    L = S->lua_state();
#else
    L = luaL_newstate();
    if (!L)
        return false;
    luaL_openlibs(L);
#endif
    return register_api();
#else
    return false;
#endif
}

#ifdef GUB_ENABLE_LUA
static LuaManager* g_mgr = nullptr;
static State* g_state_ctx = nullptr;
static Entity* g_player_ctx = nullptr;
#endif

#if defined(GUB_ENABLE_LUA) && !defined(GUB_USE_SOL2)
static int l_api_add_plate(lua_State* Ls) {
    int n = 1;
    if (lua_gettop(Ls) >= 1 && lua_isnumber(Ls, 1))
        n = (int)lua_tointeger(Ls, 1);
    if (g_player_ctx) {
        g_player_ctx->stats.plates += n;
        if (g_player_ctx->stats.plates < 0)
            g_player_ctx->stats.plates = 0;
    }
    return 0;
}
static int l_api_heal(lua_State* Ls) {
    int n = 0;
    if (lua_gettop(Ls) >= 1 && lua_isnumber(Ls, 1))
        n = (int)lua_tointeger(Ls, 1);
    if (g_player_ctx) {
        std::uint32_t hp = g_player_ctx->health;
        std::uint32_t mx = g_player_ctx->max_hp;
        if (mx == 0)
            mx = 1000;
        std::uint32_t nhp = hp + (n > 0 ? (std::uint32_t)n : 0u);
        if (nhp > mx)
            nhp = mx;
        g_player_ctx->health = nhp;
    }
    return 0;
}
static int l_api_add_move_speed(lua_State* Ls) {
    int n = 0;
    if (lua_gettop(Ls) >= 1 && lua_isnumber(Ls, 1))
        n = (int)lua_tointeger(Ls, 1);
    if (g_player_ctx) {
        g_player_ctx->stats.move_speed += (float)n;
    }
    return 0;
}

static int l_register_powerup(lua_State* L) {
    if (!lua_istable(L, 1))
        return 0;
    PowerupDef d{};
    lua_getfield(L, 1, "name");
    if (lua_isstring(L, -1))
        d.name = lua_tostring(L, -1);
    lua_pop(L, 1);
    lua_getfield(L, 1, "type");
    if (lua_isinteger(L, -1))
        d.type = (int)lua_tointeger(L, -1);
    lua_pop(L, 1);
    lua_getfield(L, 1, "sprite");
    if (lua_isstring(L, -1))
        d.sprite = lua_tostring(L, -1);
    lua_pop(L, 1);
    if (g_mgr)
        g_mgr->add_powerup(d);
    return 0;
}
static int l_register_item(lua_State* L) {
    if (!lua_istable(L, 1))
        return 0;
    ItemDef d{};
    lua_getfield(L, 1, "name");
    if (lua_isstring(L, -1))
        d.name = lua_tostring(L, -1);
    lua_pop(L, 1);
    lua_getfield(L, 1, "type");
    if (lua_isinteger(L, -1))
        d.type = (int)lua_tointeger(L, -1);
    lua_pop(L, 1);
    lua_getfield(L, 1, "category");
    if (lua_isinteger(L, -1))
        d.category = (int)lua_tointeger(L, -1);
    lua_pop(L, 1);
    lua_getfield(L, 1, "max_count");
    if (lua_isinteger(L, -1))
        d.max_count = (int)lua_tointeger(L, -1);
    lua_pop(L, 1);
    lua_getfield(L, 1, "consume_on_use");
    if (lua_isboolean(L, -1))
        d.consume_on_use = lua_toboolean(L, -1) != 0;
    lua_pop(L, 1);
    lua_getfield(L, 1, "sprite");
    if (lua_isstring(L, -1))
        d.sprite = lua_tostring(L, -1);
    lua_pop(L, 1);
    lua_getfield(L, 1, "desc");
    if (lua_isstring(L, -1))
        d.desc = lua_tostring(L, -1);
    lua_pop(L, 1);
    // optional callbacks
    int on_use_ref = -1;
    lua_getfield(L, 1, "on_use");
    if (lua_isfunction(L, -1)) {
        on_use_ref = luaL_ref(L, LUA_REGISTRYINDEX);
    } else {
        lua_pop(L, 1);
    }
    d.on_use_ref = on_use_ref;
    int on_tick_ref = -1;
    lua_getfield(L, 1, "on_tick");
    if (lua_isfunction(L, -1)) {
        on_tick_ref = luaL_ref(L, LUA_REGISTRYINDEX);
    } else {
        lua_pop(L, 1);
    }
    d.on_tick_ref = on_tick_ref;
    int on_shoot_ref = -1;
    lua_getfield(L, 1, "on_shoot");
    if (lua_isfunction(L, -1)) {
        on_shoot_ref = luaL_ref(L, LUA_REGISTRYINDEX);
    } else {
        lua_pop(L, 1);
    }
    d.on_shoot_ref = on_shoot_ref;
    int on_damage_ref = -1;
    lua_getfield(L, 1, "on_damage");
    if (lua_isfunction(L, -1)) {
        on_damage_ref = luaL_ref(L, LUA_REGISTRYINDEX);
    } else {
        lua_pop(L, 1);
    }
    d.on_damage_ref = on_damage_ref;
    if (g_mgr)
        g_mgr->add_item(d);
    return 0;
}
static int l_register_gun(lua_State* L) {
    if (!lua_istable(L, 1))
        return 0;
    GunDef d{};
    lua_getfield(L, 1, "name");
    if (lua_isstring(L, -1))
        d.name = lua_tostring(L, -1);
    lua_pop(L, 1);
    lua_getfield(L, 1, "type");
    if (lua_isinteger(L, -1))
        d.type = (int)lua_tointeger(L, -1);
    lua_pop(L, 1);
    lua_getfield(L, 1, "damage");
    if (lua_isnumber(L, -1))
        d.damage = (float)lua_tonumber(L, -1);
    lua_pop(L, 1);
    lua_getfield(L, 1, "rpm");
    if (lua_isnumber(L, -1))
        d.rpm = (float)lua_tonumber(L, -1);
    lua_pop(L, 1);
    lua_getfield(L, 1, "recoil");
    if (lua_isnumber(L, -1))
        d.recoil = (float)lua_tonumber(L, -1);
    lua_pop(L, 1);
    lua_getfield(L, 1, "control");
    if (lua_isnumber(L, -1))
        d.control = (float)lua_tonumber(L, -1);
    lua_pop(L, 1);
    lua_getfield(L, 1, "mag");
    if (lua_isinteger(L, -1))
        d.mag = (int)lua_tointeger(L, -1);
    lua_pop(L, 1);
    lua_getfield(L, 1, "ammo_max");
    if (lua_isinteger(L, -1))
        d.ammo_max = (int)lua_tointeger(L, -1);
    lua_pop(L, 1);
    if (g_mgr)
        g_mgr->add_gun(d);
    return 0;
}
#endif // GUB_ENABLE_LUA && !GUB_USE_SOL2

bool LuaManager::register_api() {
#ifdef GUB_ENABLE_LUA
    g_mgr = this;
#ifdef GUB_USE_SOL2
    sol::state& s = *S;
    s.set_function("register_powerup", [this](sol::table t) {
        PowerupDef d{};
        d.name = t.get_or("name", std::string{});
        d.type = t.get_or("type", 0);
        d.sprite = t.get_or("sprite", std::string{});
        add_powerup(d);
    });
    s.set_function("register_item", [this](sol::table t) {
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
        sol::object onuse = t.get<sol::object>("on_use");
        if (onuse.is<sol::function>()) {
            sol::function f = onuse.as<sol::function>();
            f.push();
            d.on_use_ref = luaL_ref(L, LUA_REGISTRYINDEX);
        }
        d.tick_rate_hz = t.get_or("tick_rate_hz", 0.0f);
        d.tick_phase = t.get_or("tick_phase", std::string("after"));
        sol::object onar = t.get<sol::object>("on_active_reload");
        if (onar.is<sol::function>()) {
            sol::function f = onar.as<sol::function>();
            f.push();
            d.on_active_reload_ref = luaL_ref(L, LUA_REGISTRYINDEX);
        }
        sol::object onfar = t.get<sol::object>("on_failed_active_reload");
        if (onfar.is<sol::function>()) {
            sol::function f = onfar.as<sol::function>();
            f.push();
            d.on_failed_active_reload_ref = luaL_ref(L, LUA_REGISTRYINDEX);
        }
        sol::object ontaf = t.get<sol::object>("on_tried_to_active_reload_after_failing");
        if (ontaf.is<sol::function>()) {
            sol::function f = ontaf.as<sol::function>();
            f.push();
            d.on_tried_after_failed_ar_ref = luaL_ref(L, LUA_REGISTRYINDEX);
        }
        sol::object onpu = t.get<sol::object>("on_pickup");
        if (onpu.is<sol::function>()) {
            sol::function f = onpu.as<sol::function>();
            f.push();
            d.on_pickup_ref = luaL_ref(L, LUA_REGISTRYINDEX);
        }
        sol::object ondr = t.get<sol::object>("on_drop");
        if (ondr.is<sol::function>()) {
            sol::function f = ondr.as<sol::function>();
            f.push();
            d.on_drop_ref = luaL_ref(L, LUA_REGISTRYINDEX);
        }
        add_item(d);
    });
    s.set_function("register_gun", [this](sol::table t) {
        GunDef d{};
        d.name = t.get_or("name", std::string{});
        d.type = t.get_or("type", 0);
        d.damage = t.get_or("damage", 0.0f);
        d.rpm = t.get_or("rpm", 0.0f);
        d.recoil = t.get_or("recoil", 0.0f);
        d.control = t.get_or("control", 0.0f);
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
        d.ar_size =
            t.get_or("ar_size", d.active_reload_window > 0.0f ? d.active_reload_window : 0.15f);
        d.ar_size_variance = t.get_or("ar_size_variance", 0.0f);
        d.ar_pos = t.get_or("ar_pos", 0.5f);
        // Additional gun reload hooks
        sol::object onej = t.get<sol::object>("on_eject");
        if (onej.is<sol::function>()) {
            sol::function f = onej.as<sol::function>();
            f.push();
            d.on_eject_ref = luaL_ref(L, LUA_REGISTRYINDEX);
        }
        sol::object orls = t.get<sol::object>("on_reload_start");
        if (orls.is<sol::function>()) {
            sol::function f = orls.as<sol::function>();
            f.push();
            d.on_reload_start_ref = luaL_ref(L, LUA_REGISTRYINDEX);
        }
        sol::object orlf = t.get<sol::object>("on_reload_finish");
        if (orlf.is<sol::function>()) {
            sol::function f = orlf.as<sol::function>();
            f.push();
            d.on_reload_finish_ref = luaL_ref(L, LUA_REGISTRYINDEX);
        }
        d.ar_pos_variance = t.get_or("ar_pos_variance", 0.0f);
        sol::object onjam = t.get<sol::object>("on_jam");
        if (onjam.is<sol::function>()) {
            sol::function f = onjam.as<sol::function>();
            f.push();
            d.on_jam_ref = luaL_ref(L, LUA_REGISTRYINDEX);
        }
        // already captured above; now parse on_active_reload
        sol::object onar = t.get<sol::object>("on_active_reload");
        if (onar.is<sol::function>()) {
            sol::function f = onar.as<sol::function>();
            f.push();
            d.on_active_reload_ref = luaL_ref(L, LUA_REGISTRYINDEX);
        }
        sol::object onfar = t.get<sol::object>("on_failed_active_reload");
        if (onfar.is<sol::function>()) {
            sol::function f = onfar.as<sol::function>();
            f.push();
            d.on_failed_active_reload_ref = luaL_ref(L, LUA_REGISTRYINDEX);
        }
        sol::object ontaf = t.get<sol::object>("on_tried_to_active_reload_after_failing");
        if (ontaf.is<sol::function>()) {
            sol::function f = ontaf.as<sol::function>();
            f.push();
            d.on_tried_after_failed_ar_ref = luaL_ref(L, LUA_REGISTRYINDEX);
        }
        sol::object onpu = t.get<sol::object>("on_pickup");
        if (onpu.is<sol::function>()) {
            sol::function f = onpu.as<sol::function>();
            f.push();
            d.on_pickup_ref = luaL_ref(L, LUA_REGISTRYINDEX);
        }
        sol::object ondr = t.get<sol::object>("on_drop");
        if (ondr.is<sol::function>()) {
            sol::function f = ondr.as<sol::function>();
            f.push();
            d.on_drop_ref = luaL_ref(L, LUA_REGISTRYINDEX);
        }
        sol::object ongs = t.get<sol::object>("on_step");
        if (ongs.is<sol::function>()) {
            sol::function f = ongs.as<sol::function>();
            f.push();
            d.on_step_ref = luaL_ref(L, LUA_REGISTRYINDEX);
        }
        add_gun(d);
    });
    s.set_function("register_projectile", [this](sol::table t) {
        ProjectileDef d{};
        d.name = t.get_or("name", std::string{});
        d.type = t.get_or("type", 0);
        d.speed = t.get_or("speed", 20.0f);
        d.size_x = t.get_or("size_x", 0.2f);
        d.size_y = t.get_or("size_y", 0.2f);
        d.physics_steps = t.get_or("physics_steps", 2);
        d.sprite = t.get_or("sprite", std::string{});
        sol::object ohe = t.get<sol::object>("on_hit_entity");
        if (ohe.is<sol::function>()) {
            sol::function f = ohe.as<sol::function>();
            f.push();
            d.on_hit_entity_ref = luaL_ref(L, LUA_REGISTRYINDEX);
        }
        sol::object oht = t.get<sol::object>("on_hit_tile");
        if (oht.is<sol::function>()) {
            sol::function f = oht.as<sol::function>();
            f.push();
            d.on_hit_tile_ref = luaL_ref(L, LUA_REGISTRYINDEX);
        }
        add_projectile(d);
    });
    s.set_function("register_crate", [this](sol::table t) {
        CrateDef d{};
        d.name = t.get_or("name", std::string{});
        d.type = t.get_or("type", 0);
        d.open_time = t.get_or("open_time", 5.0f);
        d.label = t.get_or("label", std::string{});
        // optional on_open
        sol::object onopen = t.get<sol::object>("on_open");
        if (onopen.is<sol::function>()) {
            sol::function f = onopen.as<sol::function>();
            f.push();
            d.on_open_ref = luaL_ref(L, LUA_REGISTRYINDEX);
        }
        // optional drops table
        sol::object dopts = t.get<sol::object>("drops");
        if (dopts.is<sol::table>()) {
            sol::table dt = dopts;
            auto parse_list = [&](const char* key, std::vector<DropEntry>& out) {
                sol::object arr = dt.get<sol::object>(key);
                if (!arr.is<sol::table>())
                    return;
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
        add_crate(d);
    });
    auto api = S->create_named_table("api");
    api.set_function("add_plate", [](int n) {
        if (g_player_ctx) {
            g_player_ctx->stats.plates += n;
            if (g_player_ctx->stats.plates < 0)
                g_player_ctx->stats.plates = 0;
        }
    });
    api.set_function("heal", [](int n) {
        if (g_player_ctx) {
            std::uint32_t hp = g_player_ctx->health;
            std::uint32_t mxhp = g_player_ctx->max_hp;
            if (mxhp == 0)
                mxhp = 1000;
            std::uint32_t add = (n > 0 ? (std::uint32_t)n : 0u);
            std::uint32_t nhp = hp + add;
            if (nhp > mxhp)
                nhp = mxhp;
            g_player_ctx->health = nhp;
        }
    });
    api.set_function("add_move_speed", [](int n) {
        if (g_player_ctx) {
            g_player_ctx->stats.move_speed += (float)n;
        }
    });
    // Dash helpers: set max and current stocks
    api.set_function("set_dash_max", [](int n) {
        if (g_state_ctx) {
            if (n < 0)
                n = 0;
            g_state_ctx->dash_max = n;
            if (g_state_ctx->dash_stocks > g_state_ctx->dash_max)
                g_state_ctx->dash_stocks = g_state_ctx->dash_max;
        }
    });
    api.set_function("set_dash_stocks", [](int n) {
        if (g_state_ctx) {
            if (n < 0)
                n = 0;
            if (n > g_state_ctx->dash_max)
                n = g_state_ctx->dash_max;
            g_state_ctx->dash_stocks = n;
        }
    });
    api.set_function("add_dash_stocks", [](int n) {
        if (g_state_ctx) {
            int v = g_state_ctx->dash_stocks + n;
            if (v < 0)
                v = 0;
            if (v > g_state_ctx->dash_max)
                v = g_state_ctx->dash_max;
            g_state_ctx->dash_stocks = v;
        }
    });
    // Optional registered on_dash handler (alternative to global function)
    s.set_function("register_on_dash", [this](sol::function f) {
        f.push();
        on_dash_ref = luaL_ref(L, LUA_REGISTRYINDEX);
    });
    s.set_function("register_on_active_reload", [this](sol::function f) {
        f.push();
        on_active_reload_ref = luaL_ref(L, LUA_REGISTRYINDEX);
    });
    s.set_function("register_on_step", [this](sol::function f) {
        f.push();
        on_step_ref = luaL_ref(L, LUA_REGISTRYINDEX);
    });
    s.set_function("register_on_failed_active_reload", [this](sol::function f) {
        f.push();
        on_failed_active_reload_ref = luaL_ref(L, LUA_REGISTRYINDEX);
    });
    s.set_function("register_on_tried_to_active_reload_after_failing", [this](sol::function f) {
        f.push();
        on_tried_after_failed_ar_ref = luaL_ref(L, LUA_REGISTRYINDEX);
    });
    s.set_function("register_on_eject", [this](sol::function f) {
        f.push();
        on_eject_ref = luaL_ref(L, LUA_REGISTRYINDEX);
    });
    s.set_function("register_on_reload_start", [this](sol::function f) {
        f.push();
        on_reload_start_ref = luaL_ref(L, LUA_REGISTRYINDEX);
    });
    s.set_function("register_on_reload_finish", [this](sol::function f) {
        f.push();
        on_reload_finish_ref = luaL_ref(L, LUA_REGISTRYINDEX);
    });
    s.set_function("register_on_active_reload", [this](sol::function f) {
        f.push();
        on_active_reload_ref = luaL_ref(L, LUA_REGISTRYINDEX);
    });
    api.set_function("refill_ammo", []() {
        if (!g_state_ctx || !g_player_ctx)
            return;
        if (!g_player_ctx->equipped_gun_vid.has_value())
            return;
        auto* gi = g_state_ctx->guns.get(*g_player_ctx->equipped_gun_vid);
        if (!gi)
            return;
        const GunDef* gd = nullptr;
        if (g_mgr) {
            for (auto const& g : g_mgr->guns())
                if (g.type == gi->def_type) {
                    gd = &g;
                    break;
                }
        }
        if (!gd)
            return;
        gi->ammo_reserve = gd->ammo_max;
        gi->current_mag = gd->mag;
    });
    // World spawn helpers (require g_state_ctx)
    api.set_function("spawn_crate", [this](int type, float x, float y) {
        if (!g_state_ctx)
            return;
        auto pos = glm::vec2{x, y};
        auto tile_blocks = [&](int tx, int ty) {
            return !g_state_ctx->stage.in_bounds(tx, ty) ||
                   g_state_ctx->stage.at(tx, ty).blocks_entities();
        };
        auto nearest = [&](int tx, int ty) {
            if (!tile_blocks(tx, ty))
                return glm::ivec2{tx, ty};
            for (int r = 1; r <= 16; ++r) {
                for (int dy = -r; dy <= r; ++dy) {
                    int yy = ty + dy;
                    int dx = r - std::abs(dy);
                    for (int sx : {-dx, dx}) {
                        int xx = tx + sx;
                        if (!tile_blocks(xx, yy))
                            return glm::ivec2{xx, yy};
                    }
                }
            }
            return glm::ivec2{tx, ty};
        };
        glm::ivec2 t{(int)std::floor(pos.x), (int)std::floor(pos.y)};
        auto w = nearest(t.x, t.y);
        glm::vec2 safe{(float)w.x + 0.5f, (float)w.y + 0.5f};
        g_state_ctx->crates.spawn(safe, type);
    });
    api.set_function("spawn_crate_safe", [this](int type, float x, float y) {
        if (!g_state_ctx)
            return;
        auto pos = glm::vec2{x, y};
        // push out of impassable tiles to nearest walkable tile center
        auto tile_blocks = [&](int tx, int ty) {
            return !g_state_ctx->stage.in_bounds(tx, ty) ||
                   g_state_ctx->stage.at(tx, ty).blocks_entities();
        };
        auto nearest = [&](int tx, int ty) {
            if (!tile_blocks(tx, ty))
                return glm::ivec2{tx, ty};
            for (int r = 1; r <= 16; ++r) {
                for (int dy = -r; dy <= r; ++dy) {
                    int yy = ty + dy;
                    int dx = r - std::abs(dy);
                    for (int sx : {-dx, dx}) {
                        int xx = tx + sx;
                        if (!tile_blocks(xx, yy))
                            return glm::ivec2{xx, yy};
                    }
                }
            }
            return glm::ivec2{tx, ty};
        };
        glm::ivec2 t{(int)std::floor(pos.x), (int)std::floor(pos.y)};
        auto w = nearest(t.x, t.y);
        glm::vec2 safe{(float)w.x + 0.5f, (float)w.y + 0.5f};
        g_state_ctx->crates.spawn(safe, type);
    });
    api.set_function("spawn_item", [this](int type, int count, float x, float y) {
        if (!g_state_ctx || !g_mgr)
            return;
        const ItemDef* id = nullptr;
        for (auto const& d : items_)
            if (d.type == type) {
                id = &d;
                break;
            }
        if (!id)
            return;
        auto iv = g_state_ctx->items.spawn_from_def(*id, (uint32_t)std::max(1, count));
        if (iv) {
            auto pos = glm::vec2{x, y};
            auto tile_blocks = [&](int tx, int ty) {
                return !g_state_ctx->stage.in_bounds(tx, ty) ||
                       g_state_ctx->stage.at(tx, ty).blocks_entities();
            };
            auto nearest = [&](int tx, int ty) {
                if (!tile_blocks(tx, ty))
                    return glm::ivec2{tx, ty};
                for (int r = 1; r <= 16; ++r) {
                    for (int dy = -r; dy <= r; ++dy) {
                        int yy = ty + dy;
                        int dx = r - std::abs(dy);
                        for (int sx : {-dx, dx}) {
                            int xx = tx + sx;
                            if (!tile_blocks(xx, yy))
                                return glm::ivec2{xx, yy};
                        }
                    }
                }
                return glm::ivec2{tx, ty};
            };
            glm::ivec2 t{(int)std::floor(pos.x), (int)std::floor(pos.y)};
            auto w = nearest(t.x, t.y);
            glm::vec2 safe{(float)w.x + 0.5f, (float)w.y + 0.5f};
            g_state_ctx->ground_items.spawn(*iv, safe);
        }
    });
    api.set_function("spawn_gun", [this](int type, float x, float y) {
        if (!g_state_ctx || !g_mgr)
            return;
        const GunDef* gd = nullptr;
        for (auto const& g : guns_)
            if (g.type == type) {
                gd = &g;
                break;
            }
        if (!gd)
            return;
        auto gv = g_state_ctx->guns.spawn_from_def(*gd);
        if (gv) {
            auto pos = glm::vec2{x, y};
            auto tile_blocks = [&](int tx, int ty) {
                return !g_state_ctx->stage.in_bounds(tx, ty) ||
                       g_state_ctx->stage.at(tx, ty).blocks_entities();
            };
            auto nearest = [&](int tx, int ty) {
                if (!tile_blocks(tx, ty))
                    return glm::ivec2{tx, ty};
                for (int r = 1; r <= 16; ++r) {
                    for (int dy = -r; dy <= r; ++dy) {
                        int yy = ty + dy;
                        int dx = r - std::abs(dy);
                        for (int sx : {-dx, dx}) {
                            int xx = tx + sx;
                            if (!tile_blocks(xx, yy))
                                return glm::ivec2{xx, yy};
                        }
                    }
                }
                return glm::ivec2{tx, ty};
            };
            glm::ivec2 t{(int)std::floor(pos.x), (int)std::floor(pos.y)};
            auto w = nearest(t.x, t.y);
            glm::vec2 safe{(float)w.x + 0.5f, (float)w.y + 0.5f};
            int sid = -1;
            g_state_ctx->ground_guns.spawn(*gv, safe, sid);
        }
    });
    return true;
#else  // C API registration
    lua_pushcfunction(L, l_register_powerup);
    lua_setglobal(L, "register_powerup");
    lua_pushcfunction(L, l_register_item);
    lua_setglobal(L, "register_item");
    lua_pushcfunction(L, l_register_gun);
    lua_setglobal(L, "register_gun");
    lua_newtable(L);
    lua_pushcfunction(L, l_api_add_plate);
    lua_setfield(L, -2, "add_plate");
    lua_pushcfunction(L, l_api_heal);
    lua_setfield(L, -2, "heal");
    lua_pushcfunction(L, l_api_add_move_speed);
    lua_setfield(L, -2, "add_move_speed");
    lua_setglobal(L, "api");
    return true;
#endif // sol2 vs C API
#else
    return false;
#endif
}

bool LuaManager::run_file(const std::string& path) {
#ifdef GUB_ENABLE_LUA
#ifdef GUB_USE_SOL2
    sol::protected_function_result r = S->safe_script_file(path);
    if (!r.valid()) {
        sol::error e = r;
        std::fprintf(stderr, "[lua] error in %s: %s\n", path.c_str(), e.what());
        return false;
    }
    return true;
#else
    if (luaL_dofile(L, path.c_str()) != LUA_OK) {
        const char* err = lua_tostring(L, -1);
        std::fprintf(stderr, "[lua] error in %s: %s\n", path.c_str(), err ? err : "(unknown)");
        lua_pop(L, 1);
        return false;
    }
    return true;
#endif
#else
    (void)path;
    return false;
#endif
}

void LuaManager::call_projectile_on_hit_entity(int proj_type) {
#ifdef GUB_ENABLE_LUA
#ifdef GUB_USE_SOL2
    const ProjectileDef* pd = find_projectile(proj_type);
    if (!pd || pd->on_hit_entity_ref < 0)
        return;
    lua_rawgeti(L, LUA_REGISTRYINDEX, pd->on_hit_entity_ref);
    if (lua_pcall(L, 0, 0, 0) != LUA_OK) {
        const char* err = lua_tostring(L, -1);
        std::fprintf(stderr, "[lua] projectile on_hit_entity error: %s\n", err ? err : "(unknown)");
        lua_pop(L, 1);
    }
#endif
#endif
}

void LuaManager::call_projectile_on_hit_tile(int proj_type) {
#ifdef GUB_ENABLE_LUA
#ifdef GUB_USE_SOL2
    const ProjectileDef* pd = find_projectile(proj_type);
    if (!pd || pd->on_hit_tile_ref < 0)
        return;
    lua_rawgeti(L, LUA_REGISTRYINDEX, pd->on_hit_tile_ref);
    if (lua_pcall(L, 0, 0, 0) != LUA_OK) {
        const char* err = lua_tostring(L, -1);
        std::fprintf(stderr, "[lua] projectile on_hit_tile error: %s\n", err ? err : "(unknown)");
        lua_pop(L, 1);
    }
#endif
#endif
}

void LuaManager::call_crate_on_open(int crate_type, State& state, Entity& player) {
#ifdef GUB_ENABLE_LUA
#ifdef GUB_USE_SOL2
    (void)state;
    (void)player;
    for (auto const& c : crates_)
        if (c.type == crate_type) {
            if (c.on_open_ref >= 0) {
                lua_rawgeti(L, LUA_REGISTRYINDEX, c.on_open_ref);
                if (lua_pcall(L, 0, 0, 0) != LUA_OK) {
                    const char* err = lua_tostring(L, -1);
                    std::fprintf(stderr, "[lua] crate on_open error: %s\n",
                                 err ? err : "(unknown)");
                    lua_pop(L, 1);
                }
            }
            break;
        }
#else
    (void)crate_type;
    (void)state;
    (void)player;
#endif
#else
    (void)crate_type;
    (void)state;
    (void)player;
#endif
}

void LuaManager::call_on_dash(State& state, Entity& player) {
#ifdef GUB_ENABLE_LUA
#ifdef GUB_USE_SOL2
    if (!S)
        return;
    g_state_ctx = &state;
    g_player_ctx = &player;
    if (on_dash_ref >= 0) {
        lua_rawgeti(L, LUA_REGISTRYINDEX, on_dash_ref);
        if (lua_pcall(L, 0, 0, 0) != LUA_OK) {
            const char* err = lua_tostring(L, -1);
            std::fprintf(stderr, "[lua] on_dash (registered) error: %s\n", err ? err : "(unknown)");
            lua_pop(L, 1);
        }
    } else {
        sol::object obj = S->get<sol::object>("on_dash");
        if (obj.is<sol::function>()) {
            sol::function f = obj.as<sol::function>();
            sol::protected_function_result r = f();
            if (!r.valid()) {
                sol::error e = r;
                std::fprintf(stderr, "[lua] error in on_dash: %s\n", e.what());
            }
        }
    }
    g_state_ctx = nullptr;
    g_player_ctx = nullptr;
#else
    (void)state;
    (void)player;
#endif
#else
    (void)state;
    (void)player;
#endif
}

void LuaManager::call_on_step(State& state, Entity* player) {
#ifdef GUB_ENABLE_LUA
#ifdef GUB_USE_SOL2
    if (!S)
        return;
    g_state_ctx = &state;
    g_player_ctx = player;
    if (on_step_ref >= 0) {
        lua_rawgeti(L, LUA_REGISTRYINDEX, on_step_ref);
        if (lua_pcall(L, 0, 0, 0) != LUA_OK) {
            const char* err = lua_tostring(L, -1);
            std::fprintf(stderr, "[lua] on_step error: %s\n", err ? err : "(unknown)");
            lua_pop(L, 1);
        }
    }
    g_state_ctx = nullptr;
    g_player_ctx = nullptr;
#else
    (void)state;
    (void)player;
#endif
#else
    (void)state;
    (void)player;
#endif
}

void LuaManager::call_on_active_reload(State& state, Entity& player) {
#ifdef GUB_ENABLE_LUA
#ifdef GUB_USE_SOL2
    if (!S)
        return;
    g_state_ctx = &state;
    g_player_ctx = &player;
    if (on_active_reload_ref >= 0) {
        lua_rawgeti(L, LUA_REGISTRYINDEX, on_active_reload_ref);
        if (lua_pcall(L, 0, 0, 0) != LUA_OK) {
            const char* err = lua_tostring(L, -1);
            std::fprintf(stderr, "[lua] on_active_reload (registered) error: %s\n",
                         err ? err : "(unknown)");
            lua_pop(L, 1);
        }
    } else {
        sol::object obj = S->get<sol::object>("on_active_reload");
        if (obj.is<sol::function>()) {
            sol::function f = obj.as<sol::function>();
            sol::protected_function_result r = f();
            if (!r.valid()) {
                sol::error e = r;
                std::fprintf(stderr, "[lua] error in on_active_reload: %s\n", e.what());
            }
        }
    }
    g_state_ctx = nullptr;
    g_player_ctx = nullptr;
#else
    (void)state;
    (void)player;
#endif
#else
    (void)state;
    (void)player;
#endif
}

void LuaManager::call_on_failed_active_reload(State& state, Entity& player) {
#ifdef GUB_ENABLE_LUA
#ifdef GUB_USE_SOL2
    if (!S)
        return;
    g_state_ctx = &state;
    g_player_ctx = &player;
    if (on_failed_active_reload_ref >= 0) {
        lua_rawgeti(L, LUA_REGISTRYINDEX, on_failed_active_reload_ref);
        if (lua_pcall(L, 0, 0, 0) != LUA_OK) {
            const char* err = lua_tostring(L, -1);
            std::fprintf(stderr,
                         "[lua] on_failed_active_reload (registered) error: %s\n",
                         err ? err : "(unknown)");
            lua_pop(L, 1);
        }
    } else {
        sol::object obj = S->get<sol::object>("on_failed_active_reload");
        if (obj.is<sol::function>()) {
            sol::function f = obj.as<sol::function>();
            auto r = f();
            if (!r.valid()) {
                sol::error e = r;
                std::fprintf(stderr, "[lua] on_failed_active_reload error: %s\n", e.what());
            }
        }
    }
    g_state_ctx = nullptr;
    g_player_ctx = nullptr;
#else
    (void)state;
    (void)player;
#endif
#else
    (void)state;
    (void)player;
#endif
}

void LuaManager::call_on_tried_after_failed_ar(State& state, Entity& player) {
#ifdef GUB_ENABLE_LUA
#ifdef GUB_USE_SOL2
    if (!S)
        return;
    g_state_ctx = &state;
    g_player_ctx = &player;
    if (on_tried_after_failed_ar_ref >= 0) {
        lua_rawgeti(L, LUA_REGISTRYINDEX, on_tried_after_failed_ar_ref);
        if (lua_pcall(L, 0, 0, 0) != LUA_OK) {
            const char* err = lua_tostring(L, -1);
            std::fprintf(stderr,
                         "[lua] on_tried_to_active_reload_after_failing (registered) error: %s\n",
                         err ? err : "(unknown)");
            lua_pop(L, 1);
        }
    } else {
        sol::object obj = S->get<sol::object>("on_tried_to_active_reload_after_failing");
        if (obj.is<sol::function>()) {
            sol::function f = obj.as<sol::function>();
            auto r = f();
            if (!r.valid()) {
                sol::error e = r;
                std::fprintf(stderr,
                             "[lua] on_tried_to_active_reload_after_failing error: %s\n",
                             e.what());
            }
        }
    }
    g_state_ctx = nullptr;
    g_player_ctx = nullptr;
#else
    (void)state;
    (void)player;
#endif
#else
    (void)state;
    (void)player;
#endif
}

void LuaManager::call_on_eject(State& state, Entity& player) {
#ifdef GUB_ENABLE_LUA
#ifdef GUB_USE_SOL2
    if (!S)
        return;
    g_state_ctx = &state;
    g_player_ctx = &player;
    if (on_eject_ref >= 0) {
        lua_rawgeti(L, LUA_REGISTRYINDEX, on_eject_ref);
        if (lua_pcall(L, 0, 0, 0) != LUA_OK) {
            const char* err = lua_tostring(L, -1);
            std::fprintf(stderr, "[lua] on_eject error: %s\n", err ? err : "(unknown)");
            lua_pop(L, 1);
        }
    } else {
        sol::object obj = S->get<sol::object>("on_eject");
        if (obj.is<sol::function>()) {
            sol::function f = obj.as<sol::function>();
            auto r = f();
            if (!r.valid()) {
                sol::error e = r;
                std::fprintf(stderr, "[lua] on_eject error: %s\n", e.what());
            }
        }
    }
    g_state_ctx = nullptr;
    g_player_ctx = nullptr;
#else
    (void)state;
    (void)player;
#endif
#else
    (void)state;
    (void)player;
#endif
}

void LuaManager::call_on_reload_start(State& state, Entity& player) {
#ifdef GUB_ENABLE_LUA
#ifdef GUB_USE_SOL2
    if (!S)
        return;
    g_state_ctx = &state;
    g_player_ctx = &player;
    if (on_reload_start_ref >= 0) {
        lua_rawgeti(L, LUA_REGISTRYINDEX, on_reload_start_ref);
        if (lua_pcall(L, 0, 0, 0) != LUA_OK) {
            const char* err = lua_tostring(L, -1);
            std::fprintf(stderr, "[lua] on_reload_start error: %s\n", err ? err : "(unknown)");
            lua_pop(L, 1);
        }
    } else {
        sol::object obj = S->get<sol::object>("on_reload_start");
        if (obj.is<sol::function>()) {
            sol::function f = obj.as<sol::function>();
            auto r = f();
            if (!r.valid()) {
                sol::error e = r;
                std::fprintf(stderr, "[lua] on_reload_start error: %s\n", e.what());
            }
        }
    }
    g_state_ctx = nullptr;
    g_player_ctx = nullptr;
#else
    (void)state;
    (void)player;
#endif
#else
    (void)state;
    (void)player;
#endif
}

void LuaManager::call_on_reload_finish(State& state, Entity& player) {
#ifdef GUB_ENABLE_LUA
#ifdef GUB_USE_SOL2
    if (!S)
        return;
    g_state_ctx = &state;
    g_player_ctx = &player;
    if (on_reload_finish_ref >= 0) {
        lua_rawgeti(L, LUA_REGISTRYINDEX, on_reload_finish_ref);
        if (lua_pcall(L, 0, 0, 0) != LUA_OK) {
            const char* err = lua_tostring(L, -1);
            std::fprintf(stderr, "[lua] on_reload_finish error: %s\n", err ? err : "(unknown)");
            lua_pop(L, 1);
        }
    } else {
        sol::object obj = S->get<sol::object>("on_reload_finish");
        if (obj.is<sol::function>()) {
            sol::function f = obj.as<sol::function>();
            auto r = f();
            if (!r.valid()) {
                sol::error e = r;
                std::fprintf(stderr, "[lua] on_reload_finish error: %s\n", e.what());
            }
        }
    }
    g_state_ctx = nullptr;
    g_player_ctx = nullptr;
#else
    (void)state;
    (void)player;
#endif
#else
    (void)state;
    (void)player;
#endif
}

void LuaManager::call_gun_on_eject(int gun_type, State& state, Entity& player) {
#ifdef GUB_ENABLE_LUA
#ifdef GUB_USE_SOL2
    (void)state;
    (void)player;
    const GunDef* gd = find_gun_def_by_type(guns_, gun_type);
    if (!gd || gd->on_eject_ref < 0)
        return;
    lua_rawgeti(L, LUA_REGISTRYINDEX, gd->on_eject_ref);
    if (lua_pcall(L, 0, 0, 0) != LUA_OK) {
        const char* err = lua_tostring(L, -1);
        std::fprintf(stderr, "[lua] gun on_eject error: %s\n", err ? err : "(unknown)");
        lua_pop(L, 1);
    }
#else
    (void)gun_type;
    (void)state;
    (void)player;
#endif
#else
    (void)gun_type;
    (void)state;
    (void)player;
#endif
}

void LuaManager::call_gun_on_reload_start(int gun_type, State& state, Entity& player) {
#ifdef GUB_ENABLE_LUA
#ifdef GUB_USE_SOL2
    (void)state;
    (void)player;
    const GunDef* gd = find_gun_def_by_type(guns_, gun_type);
    if (!gd || gd->on_reload_start_ref < 0)
        return;
    lua_rawgeti(L, LUA_REGISTRYINDEX, gd->on_reload_start_ref);
    if (lua_pcall(L, 0, 0, 0) != LUA_OK) {
        const char* err = lua_tostring(L, -1);
        std::fprintf(stderr, "[lua] gun on_reload_start error: %s\n", err ? err : "(unknown)");
        lua_pop(L, 1);
    }
#else
    (void)gun_type;
    (void)state;
    (void)player;
#endif
#else
    (void)gun_type;
    (void)state;
    (void)player;
#endif
}

void LuaManager::call_gun_on_reload_finish(int gun_type, State& state, Entity& player) {
#ifdef GUB_ENABLE_LUA
#ifdef GUB_USE_SOL2
    (void)state;
    (void)player;
    const GunDef* gd = find_gun_def_by_type(guns_, gun_type);
    if (!gd || gd->on_reload_finish_ref < 0)
        return;
    lua_rawgeti(L, LUA_REGISTRYINDEX, gd->on_reload_finish_ref);
    if (lua_pcall(L, 0, 0, 0) != LUA_OK) {
        const char* err = lua_tostring(L, -1);
        std::fprintf(stderr, "[lua] gun on_reload_finish error: %s\n", err ? err : "(unknown)");
        lua_pop(L, 1);
    }
#else
    (void)gun_type;
    (void)state;
    (void)player;
#endif
#else
    (void)gun_type;
    (void)state;
    (void)player;
#endif
}

void LuaManager::call_item_on_eject(int item_type, State& state, Entity& player) {
#ifdef GUB_ENABLE_LUA
#ifdef GUB_USE_SOL2
    const ItemDef* def = nullptr;
    for (auto const& d : items_)
        if (d.type == item_type) {
            def = &d;
            break;
        }
    if (!def || def->on_eject_ref < 0)
        return;
    g_state_ctx = &state;
    g_player_ctx = &player;
    lua_rawgeti(L, LUA_REGISTRYINDEX, def->on_eject_ref);
    if (lua_pcall(L, 0, 0, 0) != LUA_OK) {
        const char* err = lua_tostring(L, -1);
        std::fprintf(stderr, "[lua] item on_eject error: %s\n", err ? err : "(unknown)");
        lua_pop(L, 1);
    }
    g_state_ctx = nullptr;
    g_player_ctx = nullptr;
#else
    (void)item_type;
    (void)state;
    (void)player;
#endif
#else
    (void)item_type;
    (void)state;
    (void)player;
#endif
}

void LuaManager::call_item_on_reload_start(int item_type, State& state, Entity& player) {
#ifdef GUB_ENABLE_LUA
#ifdef GUB_USE_SOL2
    const ItemDef* def = nullptr;
    for (auto const& d : items_)
        if (d.type == item_type) {
            def = &d;
            break;
        }
    if (!def || def->on_reload_start_ref < 0)
        return;
    g_state_ctx = &state;
    g_player_ctx = &player;
    lua_rawgeti(L, LUA_REGISTRYINDEX, def->on_reload_start_ref);
    if (lua_pcall(L, 0, 0, 0) != LUA_OK) {
        const char* err = lua_tostring(L, -1);
        std::fprintf(stderr, "[lua] item on_reload_start error: %s\n", err ? err : "(unknown)");
        lua_pop(L, 1);
    }
    g_state_ctx = nullptr;
    g_player_ctx = nullptr;
#else
    (void)item_type;
    (void)state;
    (void)player;
#endif
#else
    (void)item_type;
    (void)state;
    (void)player;
#endif
}

void LuaManager::call_item_on_reload_finish(int item_type, State& state, Entity& player) {
#ifdef GUB_ENABLE_LUA
#ifdef GUB_USE_SOL2
    const ItemDef* def = nullptr;
    for (auto const& d : items_)
        if (d.type == item_type) {
            def = &d;
            break;
        }
    if (!def || def->on_reload_finish_ref < 0)
        return;
    g_state_ctx = &state;
    g_player_ctx = &player;
    lua_rawgeti(L, LUA_REGISTRYINDEX, def->on_reload_finish_ref);
    if (lua_pcall(L, 0, 0, 0) != LUA_OK) {
        const char* err = lua_tostring(L, -1);
        std::fprintf(stderr, "[lua] item on_reload_finish error: %s\n", err ? err : "(unknown)");
        lua_pop(L, 1);
    }
    g_state_ctx = nullptr;
    g_player_ctx = nullptr;
#else
    (void)item_type;
    (void)state;
    (void)player;
#endif
#else
    (void)item_type;
    (void)state;
    (void)player;
#endif
}

void LuaManager::call_gun_on_active_reload(int gun_type, State& state, Entity& player) {
#ifdef GUB_ENABLE_LUA
#ifdef GUB_USE_SOL2
    (void)state;
    (void)player;
    const GunDef* gd = find_gun_def_by_type(guns_, gun_type);
    if (!gd || gd->on_active_reload_ref < 0)
        return;
    lua_rawgeti(L, LUA_REGISTRYINDEX, gd->on_active_reload_ref);
    if (lua_pcall(L, 0, 0, 0) != LUA_OK) {
        const char* err = lua_tostring(L, -1);
        std::fprintf(stderr, "[lua] on_active_reload gun error: %s\n", err ? err : "(unknown)");
        lua_pop(L, 1);
    }
#else
    (void)gun_type;
    (void)state;
    (void)player;
#endif
#else
    (void)gun_type;
    (void)state;
    (void)player;
#endif
}

void LuaManager::call_gun_on_failed_active_reload(int gun_type, State& state, Entity& player) {
#ifdef GUB_ENABLE_LUA
#ifdef GUB_USE_SOL2
    (void)state;
    (void)player;
    const GunDef* gd = find_gun_def_by_type(guns_, gun_type);
    if (!gd || gd->on_failed_active_reload_ref < 0)
        return;
    lua_rawgeti(L, LUA_REGISTRYINDEX, gd->on_failed_active_reload_ref);
    if (lua_pcall(L, 0, 0, 0) != LUA_OK) {
        const char* err = lua_tostring(L, -1);
        std::fprintf(stderr, "[lua] on_failed_active_reload gun error: %s\n",
                     err ? err : "(unknown)");
        lua_pop(L, 1);
    }
#else
    (void)gun_type;
    (void)state;
    (void)player;
#endif
#else
    (void)gun_type;
    (void)state;
    (void)player;
#endif
}

void LuaManager::call_gun_on_tried_after_failed_ar(int gun_type, State& state, Entity& player) {
#ifdef GUB_ENABLE_LUA
#ifdef GUB_USE_SOL2
    (void)state;
    (void)player;
    const GunDef* gd = find_gun_def_by_type(guns_, gun_type);
    if (!gd || gd->on_tried_after_failed_ar_ref < 0)
        return;
    lua_rawgeti(L, LUA_REGISTRYINDEX, gd->on_tried_after_failed_ar_ref);
    if (lua_pcall(L, 0, 0, 0) != LUA_OK) {
        const char* err = lua_tostring(L, -1);
        std::fprintf(stderr,
                     "[lua] on_tried_to_active_reload_after_failing gun error: %s\n",
                     err ? err : "(unknown)");
        lua_pop(L, 1);
    }
#else
    (void)gun_type;
    (void)state;
    (void)player;
#endif
#else
    (void)gun_type;
    (void)state;
    (void)player;
#endif
}

void LuaManager::call_gun_on_step(int gun_type, State& state, Entity& player) {
#ifdef GUB_ENABLE_LUA
#ifdef GUB_USE_SOL2
    (void)state;
    (void)player;
    const GunDef* gd = find_gun_def_by_type(guns_, gun_type);
    if (!gd || gd->on_step_ref < 0)
        return;
    lua_rawgeti(L, LUA_REGISTRYINDEX, gd->on_step_ref);
    if (lua_pcall(L, 0, 0, 0) != LUA_OK) {
        const char* err = lua_tostring(L, -1);
        std::fprintf(stderr, "[lua] gun on_step error: %s\n", err ? err : "(unknown)");
        lua_pop(L, 1);
    }
#else
    (void)gun_type;
    (void)state;
    (void)player;
#endif
#else
    (void)gun_type;
    (void)state;
    (void)player;
#endif
}

void LuaManager::call_item_on_active_reload(int item_type, State& state, Entity& player) {
#ifdef GUB_ENABLE_LUA
#ifdef GUB_USE_SOL2
    const ItemDef* def = nullptr;
    for (auto const& d : items_)
        if (d.type == item_type) {
            def = &d;
            break;
        }
    if (!def || def->on_active_reload_ref < 0)
        return;
    g_state_ctx = &state;
    g_player_ctx = &player;
    lua_rawgeti(L, LUA_REGISTRYINDEX, def->on_active_reload_ref);
    if (lua_pcall(L, 0, 0, 0) != LUA_OK) {
        const char* err = lua_tostring(L, -1);
        std::fprintf(stderr, "[lua] item on_active_reload error: %s\n", err ? err : "(unknown)");
        lua_pop(L, 1);
    }
    g_state_ctx = nullptr;
    g_player_ctx = nullptr;
#else
    (void)item_type;
    (void)state;
    (void)player;
#endif
#else
    (void)item_type;
    (void)state;
    (void)player;
#endif
}

void LuaManager::call_item_on_failed_active_reload(int item_type, State& state, Entity& player) {
#ifdef GUB_ENABLE_LUA
#ifdef GUB_USE_SOL2
    const ItemDef* def = nullptr;
    for (auto const& d : items_)
        if (d.type == item_type) {
            def = &d;
            break;
        }
    if (!def || def->on_failed_active_reload_ref < 0)
        return;
    g_state_ctx = &state;
    g_player_ctx = &player;
    lua_rawgeti(L, LUA_REGISTRYINDEX, def->on_failed_active_reload_ref);
    if (lua_pcall(L, 0, 0, 0) != LUA_OK) {
        const char* err = lua_tostring(L, -1);
        std::fprintf(stderr, "[lua] item on_failed_active_reload error: %s\n",
                     err ? err : "(unknown)");
        lua_pop(L, 1);
    }
    g_state_ctx = nullptr;
    g_player_ctx = nullptr;
#else
    (void)item_type;
    (void)state;
    (void)player;
#endif
#else
    (void)item_type;
    (void)state;
    (void)player;
#endif
}

void LuaManager::call_item_on_tried_after_failed_ar(int item_type, State& state, Entity& player) {
#ifdef GUB_ENABLE_LUA
#ifdef GUB_USE_SOL2
    const ItemDef* def = nullptr;
    for (auto const& d : items_)
        if (d.type == item_type) {
            def = &d;
            break;
        }
    if (!def || def->on_tried_after_failed_ar_ref < 0)
        return;
    g_state_ctx = &state;
    g_player_ctx = &player;
    lua_rawgeti(L, LUA_REGISTRYINDEX, def->on_tried_after_failed_ar_ref);
    if (lua_pcall(L, 0, 0, 0) != LUA_OK) {
        const char* err = lua_tostring(L, -1);
        std::fprintf(stderr,
                     "[lua] item on_tried_to_active_reload_after_failing error: %s\n",
                     err ? err : "(unknown)");
        lua_pop(L, 1);
    }
    g_state_ctx = nullptr;
    g_player_ctx = nullptr;
#else
    (void)item_type;
    (void)state;
    (void)player;
#endif
#else
    (void)item_type;
    (void)state;
    (void)player;
#endif
}

void LuaManager::call_gun_on_pickup(int gun_type, State& state, Entity& player) {
#ifdef GUB_ENABLE_LUA
#ifdef GUB_USE_SOL2
    (void)state;
    (void)player;
    const GunDef* gd = find_gun_def_by_type(guns_, gun_type);
    if (!gd || gd->on_pickup_ref < 0)
        return;
    lua_rawgeti(L, LUA_REGISTRYINDEX, gd->on_pickup_ref);
    if (lua_pcall(L, 0, 0, 0) != LUA_OK) {
        const char* err = lua_tostring(L, -1);
        std::fprintf(stderr, "[lua] gun on_pickup error: %s\n", err ? err : "(unknown)");
        lua_pop(L, 1);
    }
#else
    (void)gun_type;
    (void)state;
    (void)player;
#endif
#else
    (void)gun_type;
    (void)state;
    (void)player;
#endif
}

void LuaManager::call_gun_on_drop(int gun_type, State& state, Entity& player) {
#ifdef GUB_ENABLE_LUA
#ifdef GUB_USE_SOL2
    (void)state;
    (void)player;
    const GunDef* gd = find_gun_def_by_type(guns_, gun_type);
    if (!gd || gd->on_drop_ref < 0)
        return;
    lua_rawgeti(L, LUA_REGISTRYINDEX, gd->on_drop_ref);
    if (lua_pcall(L, 0, 0, 0) != LUA_OK) {
        const char* err = lua_tostring(L, -1);
        std::fprintf(stderr, "[lua] gun on_drop error: %s\n", err ? err : "(unknown)");
        lua_pop(L, 1);
    }
#else
    (void)gun_type;
    (void)state;
    (void)player;
#endif
#else
    (void)gun_type;
    (void)state;
    (void)player;
#endif
}

void LuaManager::call_item_on_pickup(int item_type, State& state, Entity& player) {
#ifdef GUB_ENABLE_LUA
#ifdef GUB_USE_SOL2
    const ItemDef* def = nullptr;
    for (auto const& d : items_)
        if (d.type == item_type) {
            def = &d;
            break;
        }
    if (!def || def->on_pickup_ref < 0)
        return;
    g_state_ctx = &state;
    g_player_ctx = &player;
    lua_rawgeti(L, LUA_REGISTRYINDEX, def->on_pickup_ref);
    if (lua_pcall(L, 0, 0, 0) != LUA_OK) {
        const char* err = lua_tostring(L, -1);
        std::fprintf(stderr, "[lua] item on_pickup error: %s\n", err ? err : "(unknown)");
        lua_pop(L, 1);
    }
    g_state_ctx = nullptr;
    g_player_ctx = nullptr;
#else
    (void)item_type;
    (void)state;
    (void)player;
#endif
#else
    (void)item_type;
    (void)state;
    (void)player;
#endif
}

void LuaManager::call_item_on_drop(int item_type, State& state, Entity& player) {
#ifdef GUB_ENABLE_LUA
#ifdef GUB_USE_SOL2
    const ItemDef* def = nullptr;
    for (auto const& d : items_)
        if (d.type == item_type) {
            def = &d;
            break;
        }
    if (!def || def->on_drop_ref < 0)
        return;
    g_state_ctx = &state;
    g_player_ctx = &player;
    lua_rawgeti(L, LUA_REGISTRYINDEX, def->on_drop_ref);
    if (lua_pcall(L, 0, 0, 0) != LUA_OK) {
        const char* err = lua_tostring(L, -1);
        std::fprintf(stderr, "[lua] item on_drop error: %s\n", err ? err : "(unknown)");
        lua_pop(L, 1);
    }
    g_state_ctx = nullptr;
    g_player_ctx = nullptr;
#else
    (void)item_type;
    (void)state;
    (void)player;
#endif
#else
    (void)item_type;
    (void)state;
    (void)player;
#endif
}

void LuaManager::call_generate_room(State& state) {
#ifdef GUB_ENABLE_LUA
#ifdef GUB_USE_SOL2
    if (!S)
        return;
    sol::object obj = S->get<sol::object>("generate_room");
    if (!obj.is<sol::function>())
        return;
    g_state_ctx = &state;
    g_player_ctx = nullptr;
    sol::function f = obj.as<sol::function>();
    sol::protected_function_result r = f();
    if (!r.valid()) {
        sol::error e = r;
        std::fprintf(stderr, "[lua] error in generate_room: %s\n", e.what());
    }
    g_state_ctx = nullptr;
#else
    (void)state;
#endif
#else
    (void)state;
#endif
}

void LuaManager::call_gun_on_jam(int gun_type, State& state, Entity& player) {
#ifdef GUB_ENABLE_LUA
#ifdef GUB_USE_SOL2
    (void)state;
    (void)player;
    const GunDef* gd = find_gun_def_by_type(guns_, gun_type);
    if (!gd || gd->on_jam_ref < 0)
        return;
    lua_rawgeti(L, LUA_REGISTRYINDEX, gd->on_jam_ref);
    if (lua_pcall(L, 0, 0, 0) != LUA_OK) {
        const char* err = lua_tostring(L, -1);
        std::fprintf(stderr, "[lua] on_jam error: %s\n", err ? err : "(unknown)");
        lua_pop(L, 1);
    }
#else
    (void)gun_type;
    (void)state;
    (void)player;
#endif
#else
    (void)gun_type;
    (void)state;
    (void)player;
#endif
}

bool LuaManager::load_mods(const std::string& mods_root) {
    clear();
#ifdef GUB_ENABLE_LUA
    std::error_code ec;
    if (!fs::exists(mods_root, ec) || !fs::is_directory(mods_root, ec))
        return false;
    for (auto const& mod : fs::directory_iterator(mods_root, ec)) {
        if (ec) {
            ec.clear();
            continue;
        }
        if (!mod.is_directory())
            continue;
        fs::path sdir = mod.path() / "scripts";
        if (fs::exists(sdir, ec) && fs::is_directory(sdir, ec)) {
            for (auto const& f : fs::directory_iterator(sdir, ec)) {
                if (ec) {
                    ec.clear();
                    continue;
                }
                if (!f.is_regular_file())
                    continue;
                auto p = f.path();
                if (p.extension() == ".lua")
                    run_file(p.string());
            }
        }
    }
    std::printf("[lua] loaded: %zu powerups, %zu items, %zu guns\n", powerups_.size(),
                items_.size(), guns_.size());
    // Optional drop tables (sol2 only)
#ifdef GUB_USE_SOL2
    drops_.powerups.clear();
    drops_.items.clear();
    drops_.guns.clear();
    if (S) {
        sol::object dobj = S->get<sol::object>("drops");
        if (dobj.is<sol::table>()) {
            sol::table dt = dobj;
            auto parse_list = [&](const char* key, std::vector<DropEntry>& out) {
                sol::object arr = dt.get<sol::object>(key);
                if (!arr.is<sol::table>())
                    return;
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
#endif
    return true;
#else
    (void)mods_root;
    return false;
#endif
}

bool LuaManager::call_item_on_use(int item_type, State& state, Entity& player,
                                  std::string* out_msg) {
#ifdef GUB_ENABLE_LUA
    const ItemDef* def = nullptr;
    for (auto const& d : items_)
        if (d.type == item_type) {
            def = &d;
            break;
        }
    if (!def || def->on_use_ref < 0)
        return false;
    g_state_ctx = &state;
    g_player_ctx = &player;
    lua_rawgeti(L, LUA_REGISTRYINDEX, def->on_use_ref);
    if (lua_pcall(L, 0, 1, 0) != LUA_OK) {
        const char* err = lua_tostring(L, -1);
        std::fprintf(stderr, "[lua] on_use error: %s\n", err ? err : "(unknown)");
        lua_pop(L, 1);
        g_state_ctx = nullptr;
        g_player_ctx = nullptr;
        return false;
    }
    if (out_msg && lua_isstring(L, -1)) {
        *out_msg = lua_tostring(L, -1);
    }
    lua_pop(L, 1);
    g_state_ctx = nullptr;
    g_player_ctx = nullptr;
    return true;
#else
    (void)item_type;
    (void)state;
    (void)player;
    (void)out_msg;
    return false;
#endif
}

void LuaManager::call_item_on_tick(int item_type, State& state, Entity& player, float dt) {
#ifdef GUB_ENABLE_LUA
    const ItemDef* def = nullptr;
    for (auto const& d : items_)
        if (d.type == item_type) {
            def = &d;
            break;
        }
    if (!def || def->on_tick_ref < 0)
        return;
    g_state_ctx = &state;
    g_player_ctx = &player;
    lua_rawgeti(L, LUA_REGISTRYINDEX, def->on_tick_ref);
    lua_pushnumber(L, (lua_Number)dt);
    if (lua_pcall(L, 1, 0, 0) != LUA_OK) {
        const char* err = lua_tostring(L, -1);
        std::fprintf(stderr, "[lua] on_tick error: %s\n", err ? err : "(unknown)");
        lua_pop(L, 1);
    }
    g_state_ctx = nullptr;
    g_player_ctx = nullptr;
#else
    (void)item_type;
    (void)state;
    (void)player;
    (void)dt;
#endif
}

void LuaManager::call_item_on_shoot(int item_type, State& state, Entity& player) {
#ifdef GUB_ENABLE_LUA
    const ItemDef* def = nullptr;
    for (auto const& d : items_)
        if (d.type == item_type) {
            def = &d;
            break;
        }
    if (!def || def->on_shoot_ref < 0)
        return;
    g_state_ctx = &state;
    g_player_ctx = &player;
    lua_rawgeti(L, LUA_REGISTRYINDEX, def->on_shoot_ref);
    if (lua_pcall(L, 0, 0, 0) != LUA_OK) {
        const char* err = lua_tostring(L, -1);
        std::fprintf(stderr, "[lua] on_shoot error: %s\n", err ? err : "(unknown)");
        lua_pop(L, 1);
    }
    g_state_ctx = nullptr;
    g_player_ctx = nullptr;
#else
    (void)item_type;
    (void)state;
    (void)player;
#endif
}

void LuaManager::call_item_on_damage(int item_type, State& state, Entity& player, int attacker_ap) {
#ifdef GUB_ENABLE_LUA
    const ItemDef* def = nullptr;
    for (auto const& d : items_)
        if (d.type == item_type) {
            def = &d;
            break;
        }
    if (!def || def->on_damage_ref < 0)
        return;
    g_state_ctx = &state;
    g_player_ctx = &player;
    lua_rawgeti(L, LUA_REGISTRYINDEX, def->on_damage_ref);
    lua_pushinteger(L, (lua_Integer)attacker_ap);
    if (lua_pcall(L, 1, 0, 0) != LUA_OK) {
        const char* err = lua_tostring(L, -1);
        std::fprintf(stderr, "[lua] on_damage error: %s\n", err ? err : "(unknown)");
        lua_pop(L, 1);
    }
    g_state_ctx = nullptr;
    g_player_ctx = nullptr;
#else
    (void)item_type;
    (void)state;
    (void)player;
    (void)attacker_ap;
#endif
}
