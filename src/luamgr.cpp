#include "luamgr.hpp"

#include "entity.hpp"
#include "state.hpp"
#include "globals.hpp"
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
// sol2 pulls in the necessary Lua headers; no direct Lua C API used here.

namespace fs = std::filesystem;

LuaManager::LuaManager() {}
LuaManager::~LuaManager() {
    if (S) {
        delete S;
        S = nullptr;
        L = nullptr;
    }
}


void LuaManager::clear() {
    powerups_.clear();
    items_.clear();
    guns_.clear();
    projectiles_.clear();
    ammo_.clear();
    entity_types_.clear();
}

bool LuaManager::init() {
    if (S)
        return true;
    S = new sol::state();
    S->open_libraries(sol::lib::base, sol::lib::math, sol::lib::package, sol::lib::string,
                      sol::lib::table, sol::lib::os);
    L = S->lua_state();
    return register_api();
}

static LuaManager* g_mgr = nullptr;
static State* g_state_ctx = nullptr;
static Entity* g_player_ctx = nullptr;

struct LuaCtxGuard {
    LuaCtxGuard(State* s, Entity* p) { g_state_ctx = s; g_player_ctx = p; }
    ~LuaCtxGuard() { g_state_ctx = nullptr; g_player_ctx = nullptr; }
};


bool LuaManager::register_api() {
    g_mgr = this;
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
        if (auto o = t.get<sol::object>("on_use"); o.is<sol::function>()) d.on_use = o.as<sol::protected_function>();
        d.tick_rate_hz = t.get_or("tick_rate_hz", 0.0f);
        d.tick_phase = t.get_or("tick_phase", std::string("after"));
        if (auto o = t.get<sol::object>("on_active_reload"); o.is<sol::function>()) d.on_active_reload = o.as<sol::protected_function>();
        if (auto o = t.get<sol::object>("on_failed_active_reload"); o.is<sol::function>()) d.on_failed_active_reload = o.as<sol::protected_function>();
        if (auto o = t.get<sol::object>("on_tried_to_active_reload_after_failing"); o.is<sol::function>()) d.on_tried_after_failed_ar = o.as<sol::protected_function>();
        if (auto o = t.get<sol::object>("on_pickup"); o.is<sol::function>()) d.on_pickup = o.as<sol::protected_function>();
        if (auto o = t.get<sol::object>("on_drop"); o.is<sol::function>()) d.on_drop = o.as<sol::protected_function>();
        add_item(d);
    });
    s.set_function("register_gun", [this](sol::table t) {
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
        d.ar_size =
            t.get_or("ar_size", d.active_reload_window > 0.0f ? d.active_reload_window : 0.15f);
        d.ar_size_variance = t.get_or("ar_size_variance", 0.0f);
        d.ar_pos = t.get_or("ar_pos", 0.5f);
        // Additional gun reload hooks
        if (auto o = t.get<sol::object>("on_eject"); o.is<sol::function>()) d.on_eject = o.as<sol::protected_function>();
        sol::object orls = t.get<sol::object>("on_reload_start");
        if (orls.is<sol::function>()) d.on_reload_start = orls.as<sol::protected_function>();
        sol::object orlf = t.get<sol::object>("on_reload_finish");
        if (orlf.is<sol::function>()) d.on_reload_finish = orlf.as<sol::protected_function>();
        d.ar_pos_variance = t.get_or("ar_pos_variance", 0.0f);
        if (auto o = t.get<sol::object>("on_jam"); o.is<sol::function>()) d.on_jam = o.as<sol::protected_function>();
        // already captured above; now parse on_active_reload
        if (auto o = t.get<sol::object>("on_active_reload"); o.is<sol::function>()) d.on_active_reload = o.as<sol::protected_function>();
        if (auto o = t.get<sol::object>("on_failed_active_reload"); o.is<sol::function>()) d.on_failed_active_reload = o.as<sol::protected_function>();
        if (auto o = t.get<sol::object>("on_tried_to_active_reload_after_failing"); o.is<sol::function>()) d.on_tried_after_failed_ar = o.as<sol::protected_function>();
        if (auto o = t.get<sol::object>("on_pickup"); o.is<sol::function>()) d.on_pickup = o.as<sol::protected_function>();
        if (auto o = t.get<sol::object>("on_drop"); o.is<sol::function>()) d.on_drop = o.as<sol::protected_function>();
        if (auto o = t.get<sol::object>("on_step"); o.is<sol::function>()) d.on_step = o.as<sol::protected_function>();
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
        add_gun(d);
    });
    // Register ammo types
    s.set_function("register_ammo", [this](sol::table t) {
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
        // Optional hooks
        if (auto o = t.get<sol::object>("on_hit"); o.is<sol::function>()) d.on_hit = o.as<sol::protected_function>();
        if (auto o = t.get<sol::object>("on_hit_entity"); o.is<sol::function>()) d.on_hit_entity = o.as<sol::protected_function>();
        if (auto o = t.get<sol::object>("on_hit_tile"); o.is<sol::function>()) d.on_hit_tile = o.as<sol::protected_function>();
        add_ammo(d);
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
        if (auto o = t.get<sol::object>("on_hit_entity"); o.is<sol::function>()) d.on_hit_entity = o.as<sol::protected_function>();
        if (auto o = t.get<sol::object>("on_hit_tile"); o.is<sol::function>()) d.on_hit_tile = o.as<sol::protected_function>();
        add_projectile(d);
    });
    // Register entity types (NPC/player-capable)
    s.set_function("register_entity_type", [this](sol::table t) {
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
        if (auto o = t.get<sol::object>("on_step"); o.is<sol::function>()) d.on_step = o.as<sol::protected_function>();
        if (auto o = t.get<sol::object>("on_damage"); o.is<sol::function>()) d.on_damage = o.as<sol::protected_function>();
        if (auto o = t.get<sol::object>("on_spawn"); o.is<sol::function>()) d.on_spawn = o.as<sol::protected_function>();
        if (auto o = t.get<sol::object>("on_death"); o.is<sol::function>()) d.on_death = o.as<sol::protected_function>();
        if (auto o = t.get<sol::object>("on_reload_start"); o.is<sol::function>()) d.on_reload_start = o.as<sol::protected_function>();
        if (auto o = t.get<sol::object>("on_reload_finish"); o.is<sol::function>()) d.on_reload_finish = o.as<sol::protected_function>();
        if (auto o = t.get<sol::object>("on_gun_jam"); o.is<sol::function>()) d.on_gun_jam = o.as<sol::protected_function>();
        if (auto o = t.get<sol::object>("on_out_of_ammo"); o.is<sol::function>()) d.on_out_of_ammo = o.as<sol::protected_function>();
        if (auto o = t.get<sol::object>("on_hp_under_50"); o.is<sol::function>()) d.on_hp_under_50 = o.as<sol::protected_function>();
        if (auto o = t.get<sol::object>("on_hp_under_25"); o.is<sol::function>()) d.on_hp_under_25 = o.as<sol::protected_function>();
        if (auto o = t.get<sol::object>("on_hp_full"); o.is<sol::function>()) d.on_hp_full = o.as<sol::protected_function>();
        if (auto o = t.get<sol::object>("on_shield_under_50"); o.is<sol::function>()) d.on_shield_under_50 = o.as<sol::protected_function>();
        if (auto o = t.get<sol::object>("on_shield_under_25"); o.is<sol::function>()) d.on_shield_under_25 = o.as<sol::protected_function>();
        if (auto o = t.get<sol::object>("on_shield_full"); o.is<sol::function>()) d.on_shield_full = o.as<sol::protected_function>();
        if (auto o = t.get<sol::object>("on_plates_lost"); o.is<sol::function>()) d.on_plates_lost = o.as<sol::protected_function>();
        if (auto o = t.get<sol::object>("on_collide_tile"); o.is<sol::function>()) d.on_collide_tile = o.as<sol::protected_function>();
        add_entity_type(d);
    });
    s.set_function("register_crate", [this](sol::table t) {
        CrateDef d{};
        d.name = t.get_or("name", std::string{});
        d.type = t.get_or("type", 0);
        d.open_time = t.get_or("open_time", 5.0f);
        d.label = t.get_or("label", std::string{});
        // optional on_open
        sol::object onopen = t.get<sol::object>("on_open");
        if (onopen.is<sol::function>()) d.on_open = onopen.as<sol::protected_function>();
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
            if (g_state_ctx && n > 0) {
                if (auto* pm = g_state_ctx->metrics_for(g_player_ctx->vid))
                    pm->plates_gained += (uint32_t)n;
            }
        }
    });
    api.set_function("heal", [](int n) {
        if (g_player_ctx) {
            uint32_t hp = g_player_ctx->health;
            uint32_t mxhp = g_player_ctx->max_hp;
            if (mxhp == 0)
                mxhp = 1000;
            uint32_t add = (n > 0 ? (uint32_t)n : 0u);
            uint32_t nhp = hp + add;
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
    s.set_function("register_on_dash", [this](sol::function f) { on_dash = sol::protected_function(f); });
    s.set_function("register_on_active_reload", [this](sol::function f) { on_active_reload = sol::protected_function(f); });
    s.set_function("register_on_step", [this](sol::function f) { on_step = sol::protected_function(f); });
    s.set_function("register_on_failed_active_reload", [this](sol::function f) { on_failed_active_reload = sol::protected_function(f); });
    s.set_function("register_on_tried_to_active_reload_after_failing", [this](sol::function f) { on_tried_after_failed_ar = sol::protected_function(f); });
    s.set_function("register_on_eject", [this](sol::function f) { on_eject = sol::protected_function(f); });
    s.set_function("register_on_reload_start", [this](sol::function f) { on_reload_start = sol::protected_function(f); });
    s.set_function("register_on_reload_finish", [this](sol::function f) { on_reload_finish = sol::protected_function(f); });
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
    api.set_function("set_equipped_ammo", [](int ammo_type) {
        if (!g_state_ctx || !g_player_ctx)
            return;
        if (!g_player_ctx->equipped_gun_vid.has_value())
            return;
        auto* gi = g_state_ctx->guns.get(*g_player_ctx->equipped_gun_vid);
        if (!gi)
            return;
        // Enforce compatibility: only allow ammo listed on gun def
        const GunDef* gd = nullptr;
        if (g_mgr) {
            for (auto const& g : g_mgr->guns())
                if (g.type == gi->def_type) { gd = &g; break; }
        }
        if (!gd)
            return;
        bool ok = false;
        for (auto const& ac : gd->compatible_ammo) {
            if (ac.type == ammo_type) { ok = true; break; }
        }
        if (ok) {
            gi->ammo_type = ammo_type;
            // UX: alert with ammo name
            if (g_mgr && g_state_ctx) {
                if (auto const* ad = g_mgr->find_ammo(ammo_type))
                    g_state_ctx->alerts.push_back({std::string("Ammo set: ") + ad->name, 0.0f, 1.2f, false});
                else
                    g_state_ctx->alerts.push_back({std::string("Ammo set: ") + std::to_string(ammo_type), 0.0f, 1.0f, false});
            }
        }
    });
    api.set_function("set_equipped_ammo_force", [](int ammo_type) {
        if (!g_state_ctx || !g_player_ctx)
            return;
        if (!g_player_ctx->equipped_gun_vid.has_value())
            return;
        if (auto* gi = g_state_ctx->guns.get(*g_player_ctx->equipped_gun_vid)) {
            gi->ammo_type = ammo_type;
            if (g_mgr && g_state_ctx) {
                if (auto const* ad = g_mgr->find_ammo(ammo_type))
                    g_state_ctx->alerts.push_back({std::string("Ammo forced: ") + ad->name, 0.0f, 1.2f, false});
                else
                    g_state_ctx->alerts.push_back({std::string("Ammo forced: ") + std::to_string(ammo_type), 0.0f, 1.0f, false});
            }
        }
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
        g_state_ctx->metrics.crates_spawned += 1;
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
        g_state_ctx->metrics.crates_spawned += 1;
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
            g_state_ctx->metrics.items_spawned += 1;
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
            g_state_ctx->metrics.guns_spawned += 1;
        }
    });
    // Spawn an entity by type at world coords (center). Safe placement.
    api.set_function("spawn_entity_safe", [this](int type, float x, float y) {
        if (!g_state_ctx)
            return;
        const EntityTypeDef* ed = find_entity_type(type);
        if (!ed) {
            if (g_state_ctx) g_state_ctx->alerts.push_back({std::string("Unknown entity type ") + std::to_string(type), 0.0f, 1.5f, false});
            return;
        }
        auto tile_blocks = [&](int tx, int ty) {
            return !g_state_ctx->stage.in_bounds(tx, ty) || g_state_ctx->stage.at(tx, ty).blocks_entities();
        };
        auto nearest = [&](int tx, int ty) {
            if (!tile_blocks(tx, ty)) return glm::ivec2{tx, ty};
            for (int r = 1; r <= 16; ++r) {
                for (int dy = -r; dy <= r; ++dy) {
                    int yy = ty + dy; int dx = r - std::abs(dy);
                    for (int sx : {-dx, dx}) { int xx = tx + sx; if (!tile_blocks(xx, yy)) return glm::ivec2{xx, yy}; }
                }
            }
            return glm::ivec2{tx, ty};
        };
        glm::ivec2 t{(int)std::floor(x), (int)std::floor(y)};
        auto w = nearest(t.x, t.y);
        glm::vec2 pos{(float)w.x + 0.5f, (float)w.y + 0.5f};
        auto vid = g_state_ctx->entities.new_entity();
        if (!vid) return;
        Entity* e = g_state_ctx->entities.get_mut(*vid);
        e->type_ = ids::ET_NPC;
        e->pos = pos;
        e->size = {ed->collider_w, ed->collider_h};
        e->sprite_size = {ed->sprite_w, ed->sprite_h};
        e->physics_steps = std::max(1, ed->physics_steps);
        e->def_type = ed->type;
        e->sprite_id = -1;
        if (!ed->sprite.empty() && ed->sprite.find(':') != std::string::npos)
            e->sprite_id = try_get_sprite_id(ed->sprite);
        e->max_hp = ed->max_hp;
        e->health = e->max_hp;
        e->stats.shield_max = ed->shield_max;
        e->shield = ed->shield_max;
        e->stats.shield_regen = ed->shield_regen;
        e->stats.health_regen = ed->health_regen;
        e->stats.armor = ed->armor;
        e->stats.plates = ed->plates;
        e->stats.move_speed = ed->move_speed;
        e->stats.dodge = ed->dodge;
        e->stats.accuracy = ed->accuracy;
        e->stats.scavenging = ed->scavenging;
        e->stats.currency = ed->currency;
        e->stats.ammo_gain = ed->ammo_gain;
        e->stats.luck = ed->luck;
        e->stats.crit_chance = ed->crit_chance;
        e->stats.crit_damage = ed->crit_damage;
        e->stats.headshot_damage = ed->headshot_damage;
        e->stats.damage_absorb = ed->damage_absorb;
        e->stats.damage_output = ed->damage_output;
        e->stats.healing = ed->healing;
        e->stats.terror_level = ed->terror_level;
        e->stats.move_spread_inc_rate_deg_per_sec_at_base = ed->move_spread_inc_rate_deg_per_sec_at_base;
        e->stats.move_spread_decay_deg_per_sec = ed->move_spread_decay_deg_per_sec;
        e->stats.move_spread_max_deg = ed->move_spread_max_deg;
        call_entity_on_spawn(type, *e);
    });
    // Alias: always safe; emits alert on failure
    api.set_function("spawn_entity", [this](int type, float x, float y) {
        if (!g_state_ctx) return;
        const EntityTypeDef* ed = find_entity_type(type);
        if (!ed) {
            g_state_ctx->alerts.push_back({std::string("Unknown entity type ") + std::to_string(type), 0.0f, 1.5f, false});
            return;
        }
        auto tile_blocks = [&](int tx, int ty) {
            return !g_state_ctx->stage.in_bounds(tx, ty) || g_state_ctx->stage.at(tx, ty).blocks_entities();
        };
        auto nearest = [&](int tx, int ty) {
            if (!tile_blocks(tx, ty)) return glm::ivec2{tx, ty};
            for (int r = 1; r <= 16; ++r) {
                for (int dy = -r; dy <= r; ++dy) {
                    int yy = ty + dy; int dx = r - std::abs(dy);
                    for (int sx : {-dx, dx}) { int xx = tx + sx; if (!tile_blocks(xx, yy)) return glm::ivec2{xx, yy}; }
                }
            }
            return glm::ivec2{tx, ty};
        };
        glm::ivec2 t{(int)std::floor(x), (int)std::floor(y)};
        auto w = nearest(t.x, t.y);
        glm::vec2 pos{(float)w.x + 0.5f, (float)w.y + 0.5f};
        auto vid = g_state_ctx->entities.new_entity();
        if (!vid) { g_state_ctx->alerts.push_back({"Entity spawn failed", 0.0f, 1.5f, false}); return; }
        Entity* e = g_state_ctx->entities.get_mut(*vid);
        e->type_ = ids::ET_NPC;
        e->pos = pos;
        e->size = {ed->collider_w, ed->collider_h};
        e->sprite_size = {ed->sprite_w, ed->sprite_h};
        e->physics_steps = std::max(1, ed->physics_steps);
        e->def_type = ed->type;
        e->sprite_id = -1;
        if (!ed->sprite.empty() && ed->sprite.find(':') != std::string::npos)
            e->sprite_id = try_get_sprite_id(ed->sprite);
        e->max_hp = ed->max_hp;
        e->health = e->max_hp;
        e->stats.shield_max = ed->shield_max;
        e->shield = ed->shield_max;
        e->stats.shield_regen = ed->shield_regen;
        e->stats.health_regen = ed->health_regen;
        e->stats.armor = ed->armor;
        e->stats.plates = ed->plates;
        e->stats.move_speed = ed->move_speed;
        e->stats.dodge = ed->dodge;
        e->stats.accuracy = ed->accuracy;
        e->stats.scavenging = ed->scavenging;
        e->stats.currency = ed->currency;
        e->stats.ammo_gain = ed->ammo_gain;
        e->stats.luck = ed->luck;
        e->stats.crit_chance = ed->crit_chance;
        e->stats.crit_damage = ed->crit_damage;
        e->stats.headshot_damage = ed->headshot_damage;
        e->stats.damage_absorb = ed->damage_absorb;
        e->stats.damage_output = ed->damage_output;
        e->stats.healing = ed->healing;
        e->stats.terror_level = ed->terror_level;
        e->stats.move_spread_inc_rate_deg_per_sec_at_base = ed->move_spread_inc_rate_deg_per_sec_at_base;
        e->stats.move_spread_decay_deg_per_sec = ed->move_spread_decay_deg_per_sec;
        e->stats.move_spread_max_deg = ed->move_spread_max_deg;
        call_entity_on_spawn(type, *e);
    });
    // Force spawn: places at exact coordinates without safe adjustment
    api.set_function("spawn_entity_force", [this](int type, float x, float y) {
        if (!g_state_ctx) return;
        const EntityTypeDef* ed = find_entity_type(type);
        if (!ed) {
            g_state_ctx->alerts.push_back({std::string("Unknown entity type ") + std::to_string(type), 0.0f, 1.5f, false});
            return;
        }
        glm::vec2 pos{x, y};
        auto vid = g_state_ctx->entities.new_entity();
        if (!vid) { g_state_ctx->alerts.push_back({"Entity spawn failed", 0.0f, 1.5f, false}); return; }
        Entity* e = g_state_ctx->entities.get_mut(*vid);
        e->type_ = ids::ET_NPC;
        e->pos = pos;
        e->size = {ed->collider_w, ed->collider_h};
        e->sprite_size = {ed->sprite_w, ed->sprite_h};
        e->physics_steps = std::max(1, ed->physics_steps);
        e->def_type = ed->type;
        e->sprite_id = -1;
        if (!ed->sprite.empty() && ed->sprite.find(':') != std::string::npos)
            e->sprite_id = try_get_sprite_id(ed->sprite);
        e->max_hp = ed->max_hp;
        e->health = e->max_hp;
        e->stats.shield_max = ed->shield_max;
        e->shield = ed->shield_max;
        e->stats.shield_regen = ed->shield_regen;
        e->stats.armor = ed->armor;
        e->stats.plates = ed->plates;
        e->stats.move_speed = ed->move_speed;
        e->stats.accuracy = ed->accuracy;
        e->stats.move_spread_inc_rate_deg_per_sec_at_base = ed->move_spread_inc_rate_deg_per_sec_at_base;
        e->stats.move_spread_decay_deg_per_sec = ed->move_spread_decay_deg_per_sec;
        e->stats.move_spread_max_deg = ed->move_spread_max_deg;
        call_entity_on_spawn(type, *e);
    });
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

void LuaManager::call_projectile_on_hit_entity(int proj_type) {
    const ProjectileDef* pd = find_projectile(proj_type);
    if (!pd || !pd->on_hit_entity.valid()) return;
    auto r = pd->on_hit_entity();
    if (!r.valid()) { sol::error e = r; std::fprintf(stderr, "[lua] projectile on_hit_entity error: %s\n", e.what()); }
}

void LuaManager::call_projectile_on_hit_tile(int proj_type) {
    const ProjectileDef* pd = find_projectile(proj_type);
    if (!pd || !pd->on_hit_tile.valid()) return;
    auto r = pd->on_hit_tile();
    if (!r.valid()) { sol::error e = r; std::fprintf(stderr, "[lua] projectile on_hit_tile error: %s\n", e.what()); }
}

void LuaManager::call_ammo_on_hit(int ammo_type) {
    const AmmoDef* ad = find_ammo(ammo_type);
    if (!ad || !ad->on_hit.valid()) return;
    auto r = ad->on_hit();
    if (!r.valid()) { sol::error e = r; std::fprintf(stderr, "[lua] ammo on_hit error: %s\n", e.what()); }
}

void LuaManager::call_ammo_on_hit_entity(int ammo_type) {
    const AmmoDef* ad = find_ammo(ammo_type);
    if (!ad || !ad->on_hit_entity.valid()) return;
    auto r = ad->on_hit_entity();
    if (!r.valid()) { sol::error e = r; std::fprintf(stderr, "[lua] ammo on_hit_entity error: %s\n", e.what()); }
}

void LuaManager::call_ammo_on_hit_tile(int ammo_type) {
    const AmmoDef* ad = find_ammo(ammo_type);
    if (!ad || !ad->on_hit_tile.valid()) return;
    auto r = ad->on_hit_tile();
    if (!r.valid()) { sol::error e = r; std::fprintf(stderr, "[lua] ammo on_hit_tile error: %s\n", e.what()); }
}

void LuaManager::call_crate_on_open(int crate_type, Entity& player) {
    (void)player;
    for (auto const& c : crates_) if (c.type == crate_type) {
        if (c.on_open.valid()) {
            auto r = c.on_open();
            if (!r.valid()) { sol::error e = r; std::fprintf(stderr, "[lua] crate on_open error: %s\n", e.what()); }
        }
        break;
    }
}

void LuaManager::call_on_dash(Entity& player) {
    LuaCtxGuard _ctx(ss, &player);
    if (on_dash.valid()) {
        auto r = on_dash(); if (!r.valid()) { sol::error e = r; std::fprintf(stderr, "[lua] on_dash error: %s\n", e.what()); }
    } else {
        sol::object obj = S->get<sol::object>("on_dash");
        if (obj.is<sol::function>()) { auto r = obj.as<sol::protected_function>()(); if (!r.valid()) { sol::error e = r; std::fprintf(stderr, "[lua] on_dash error: %s\n", e.what()); } }
    }
    (void)0;
}

void LuaManager::call_on_step(Entity* player) {
    LuaCtxGuard _ctx(ss, player);
    if (on_step.valid()) { auto r = on_step(); if (!r.valid()) { sol::error e = r; std::fprintf(stderr, "[lua] on_step error: %s\n", e.what()); } }
    (void)0;
}

void LuaManager::call_on_active_reload(Entity& player) {
    LuaCtxGuard _ctx(ss, &player);
    if (on_active_reload.valid()) { auto r = on_active_reload(); if (!r.valid()) { sol::error e = r; std::fprintf(stderr, "[lua] on_active_reload error: %s\n", e.what()); } }
    else {
        sol::object obj = S->get<sol::object>("on_active_reload");
        if (obj.is<sol::function>()) { auto r = obj.as<sol::protected_function>()(); if (!r.valid()) { sol::error e = r; std::fprintf(stderr, "[lua] on_active_reload error: %s\n", e.what()); } }
    }
    (void)0;
}

void LuaManager::call_on_failed_active_reload(Entity& player) {
    LuaCtxGuard _ctx(ss, &player);
    if (on_failed_active_reload.valid()) { auto r = on_failed_active_reload(); if (!r.valid()) { sol::error e = r; std::fprintf(stderr, "[lua] on_failed_active_reload error: %s\n", e.what()); } }
    else {
        sol::object obj = S->get<sol::object>("on_failed_active_reload");
        if (obj.is<sol::function>()) { auto r = obj.as<sol::protected_function>()(); if (!r.valid()) { sol::error e = r; std::fprintf(stderr, "[lua] on_failed_active_reload error: %s\n", e.what()); } }
    }
    (void)0;
}

void LuaManager::call_on_tried_after_failed_ar(Entity& player) {
    LuaCtxGuard _ctx(ss, &player);
    if (on_tried_after_failed_ar.valid()) { auto r = on_tried_after_failed_ar(); if (!r.valid()) { sol::error e = r; std::fprintf(stderr, "[lua] on_tried_to_active_reload_after_failing error: %s\n", e.what()); } }
    else {
        sol::object obj = S->get<sol::object>("on_tried_to_active_reload_after_failing");
        if (obj.is<sol::function>()) { auto r = obj.as<sol::protected_function>()(); if (!r.valid()) { sol::error e = r; std::fprintf(stderr, "[lua] on_tried_to_active_reload_after_failing error: %s\n", e.what()); } }
    }
    (void)0;
}

void LuaManager::call_on_eject(Entity& player) {
    LuaCtxGuard _ctx(ss, &player);
    if (on_eject.valid()) { auto r = on_eject(); if (!r.valid()) { sol::error e = r; std::fprintf(stderr, "[lua] on_eject error: %s\n", e.what()); } }
    else {
        sol::object obj = S->get<sol::object>("on_eject");
        if (obj.is<sol::function>()) { auto r = obj.as<sol::protected_function>()(); if (!r.valid()) { sol::error e = r; std::fprintf(stderr, "[lua] on_eject error: %s\n", e.what()); } }
    }
    (void)0;
}

void LuaManager::call_on_reload_start(Entity& player) {
    LuaCtxGuard _ctx(ss, &player);
    if (on_reload_start.valid()) { auto r = on_reload_start(); if (!r.valid()) { sol::error e = r; std::fprintf(stderr, "[lua] on_reload_start error: %s\n", e.what()); } }
    else {
        sol::object obj = S->get<sol::object>("on_reload_start");
        if (obj.is<sol::function>()) { auto r = obj.as<sol::protected_function>()(); if (!r.valid()) { sol::error e = r; std::fprintf(stderr, "[lua] on_reload_start error: %s\n", e.what()); } }
    }
    (void)0;
}

void LuaManager::call_on_reload_finish(Entity& player) {
    LuaCtxGuard _ctx(ss, &player);
    if (on_reload_finish.valid()) { auto r = on_reload_finish(); if (!r.valid()) { sol::error e = r; std::fprintf(stderr, "[lua] on_reload_finish error: %s\n", e.what()); } }
    else {
        sol::object obj = S->get<sol::object>("on_reload_finish");
        if (obj.is<sol::function>()) { auto r = obj.as<sol::protected_function>()(); if (!r.valid()) { sol::error e = r; std::fprintf(stderr, "[lua] on_reload_finish error: %s\n", e.what()); } }
    }
    (void)0;
}

void LuaManager::call_entity_on_step(int entity_type, Entity& e) {
    const EntityTypeDef* ed = find_entity_type(entity_type);
    if (!ed || !ed->on_step.valid()) return;
    LuaCtxGuard _ctx(ss, &e);
    auto r = ed->on_step(); if (!r.valid()) { sol::error er = r; std::fprintf(stderr, "[lua] entity on_step error: %s\n", er.what()); }
}

void LuaManager::call_entity_on_damage(int entity_type, Entity& e, int attacker_ap) {
    const EntityTypeDef* ed = find_entity_type(entity_type);
    if (!ed || !ed->on_damage.valid()) return;
    LuaCtxGuard _ctx(ss, &e);
    auto r = ed->on_damage(attacker_ap); if (!r.valid()) { sol::error er = r; std::fprintf(stderr, "[lua] entity on_damage error: %s\n", er.what()); }
}

void LuaManager::call_entity_on_spawn(int entity_type, Entity& e) {
    const EntityTypeDef* ed = find_entity_type(entity_type);
    if (!ed || !ed->on_spawn.valid()) return;
    LuaCtxGuard _ctx(ss, &e);
    auto r = ed->on_spawn(); if (!r.valid()) { sol::error er = r; std::fprintf(stderr, "[lua] entity on_spawn error: %s\n", er.what()); }
}

void LuaManager::call_entity_on_death(int entity_type, Entity& e) {
    const EntityTypeDef* ed = find_entity_type(entity_type);
    if (!ed || !ed->on_death.valid()) return;
    LuaCtxGuard _ctx(ss, &e);
    auto r = ed->on_death(); if (!r.valid()) { sol::error er = r; std::fprintf(stderr, "[lua] entity on_death error: %s\n", er.what()); }
}

void LuaManager::call_entity_on_reload_start(int entity_type, Entity& e) {
    const EntityTypeDef* ed = find_entity_type(entity_type);
    if (!ed || !ed->on_reload_start.valid()) return;
    LuaCtxGuard _ctx(ss, &e);
    auto r = ed->on_reload_start(); if (!r.valid()) { sol::error er = r; std::fprintf(stderr, "[lua] entity on_reload_start error: %s\n", er.what()); }
}
void LuaManager::call_entity_on_reload_finish(int entity_type, Entity& e) {
    const EntityTypeDef* ed = find_entity_type(entity_type);
    if (!ed || !ed->on_reload_finish.valid()) return;
    LuaCtxGuard _ctx(ss, &e);
    auto r = ed->on_reload_finish(); if (!r.valid()) { sol::error er = r; std::fprintf(stderr, "[lua] entity on_reload_finish error: %s\n", er.what()); }
}
void LuaManager::call_entity_on_gun_jam(int entity_type, Entity& e) {
    const EntityTypeDef* ed = find_entity_type(entity_type);
    if (!ed || !ed->on_gun_jam.valid()) return;
    LuaCtxGuard _ctx(ss, &e);
    auto r = ed->on_gun_jam(); if (!r.valid()) { sol::error er = r; std::fprintf(stderr, "[lua] entity on_gun_jam error: %s\n", er.what()); }
}
void LuaManager::call_entity_on_out_of_ammo(int entity_type, Entity& e) {
    const EntityTypeDef* ed = find_entity_type(entity_type);
    if (!ed || !ed->on_out_of_ammo.valid()) return;
    LuaCtxGuard _ctx(ss, &e);
    auto r = ed->on_out_of_ammo(); if (!r.valid()) { sol::error er = r; std::fprintf(stderr, "[lua] entity on_out_of_ammo error: %s\n", er.what()); }
}
void LuaManager::call_entity_on_hp_under_50(int entity_type, Entity& e) {
    const EntityTypeDef* ed = find_entity_type(entity_type);
    if (!ed || !ed->on_hp_under_50.valid()) return;
    LuaCtxGuard _ctx(ss, &e);
    auto r = ed->on_hp_under_50(); if (!r.valid()) { sol::error er = r; std::fprintf(stderr, "[lua] entity on_hp_under_50 error: %s\n", er.what()); }
}
void LuaManager::call_entity_on_hp_under_25(int entity_type, Entity& e) {
    const EntityTypeDef* ed = find_entity_type(entity_type);
    if (!ed || !ed->on_hp_under_25.valid()) return;
    LuaCtxGuard _ctx(ss, &e);
    auto r = ed->on_hp_under_25(); if (!r.valid()) { sol::error er = r; std::fprintf(stderr, "[lua] entity on_hp_under_25 error: %s\n", er.what()); }
}
void LuaManager::call_entity_on_hp_full(int entity_type, Entity& e) {
    const EntityTypeDef* ed = find_entity_type(entity_type);
    if (!ed || !ed->on_hp_full.valid()) return;
    LuaCtxGuard _ctx(ss, &e);
    auto r = ed->on_hp_full(); if (!r.valid()) { sol::error er = r; std::fprintf(stderr, "[lua] entity on_hp_full error: %s\n", er.what()); }
}
void LuaManager::call_entity_on_shield_under_50(int entity_type, Entity& e) {
    const EntityTypeDef* ed = find_entity_type(entity_type);
    if (!ed || !ed->on_shield_under_50.valid()) return;
    LuaCtxGuard _ctx(ss, &e);
    auto r = ed->on_shield_under_50(); if (!r.valid()) { sol::error er = r; std::fprintf(stderr, "[lua] entity on_shield_under_50 error: %s\n", er.what()); }
}
void LuaManager::call_entity_on_shield_under_25(int entity_type, Entity& e) {
    const EntityTypeDef* ed = find_entity_type(entity_type);
    if (!ed || !ed->on_shield_under_25.valid()) return;
    LuaCtxGuard _ctx(ss, &e);
    auto r = ed->on_shield_under_25(); if (!r.valid()) { sol::error er = r; std::fprintf(stderr, "[lua] entity on_shield_under_25 error: %s\n", er.what()); }
}
void LuaManager::call_entity_on_shield_full(int entity_type, Entity& e) {
    const EntityTypeDef* ed = find_entity_type(entity_type);
    if (!ed || !ed->on_shield_full.valid()) return;
    LuaCtxGuard _ctx(ss, &e);
    auto r = ed->on_shield_full(); if (!r.valid()) { sol::error er = r; std::fprintf(stderr, "[lua] entity on_shield_full error: %s\n", er.what()); }
}
void LuaManager::call_entity_on_plates_lost(int entity_type, Entity& e) {
    const EntityTypeDef* ed = find_entity_type(entity_type);
    if (!ed || !ed->on_plates_lost.valid()) return;
    LuaCtxGuard _ctx(ss, &e);
    auto r = ed->on_plates_lost(); if (!r.valid()) { sol::error er = r; std::fprintf(stderr, "[lua] entity on_plates_lost error: %s\n", er.what()); }
}
void LuaManager::call_entity_on_collide_tile(int entity_type, Entity& e) {
    const EntityTypeDef* ed = find_entity_type(entity_type);
    if (!ed || !ed->on_collide_tile.valid()) return;
    LuaCtxGuard _ctx(ss, &e);
    auto r = ed->on_collide_tile(); if (!r.valid()) { sol::error er = r; std::fprintf(stderr, "[lua] entity on_collide_tile error: %s\n", er.what()); }
}

void LuaManager::call_gun_on_eject(int gun_type, Entity& player) {
    (void)player;
    const GunDef* gd = find_gun_def_by_type(guns_, gun_type);
    if (!gd || !gd->on_eject.valid()) return;
    auto r = gd->on_eject(); if (!r.valid()) { sol::error e = r; std::fprintf(stderr, "[lua] gun on_eject error: %s\n", e.what()); }
    (void)gun_type;
}

void LuaManager::call_gun_on_reload_start(int gun_type, Entity& player) {
    (void)player;
    const GunDef* gd = find_gun_def_by_type(guns_, gun_type);
    if (!gd || !gd->on_reload_start.valid()) return;
    auto r = gd->on_reload_start(); if (!r.valid()) { sol::error e = r; std::fprintf(stderr, "[lua] gun on_reload_start error: %s\n", e.what()); }
    (void)gun_type;
}

void LuaManager::call_gun_on_reload_finish(int gun_type, Entity& player) {
    (void)player;
    const GunDef* gd = find_gun_def_by_type(guns_, gun_type);
    if (!gd || !gd->on_reload_finish.valid()) return;
    auto r = gd->on_reload_finish(); if (!r.valid()) { sol::error e = r; std::fprintf(stderr, "[lua] gun on_reload_finish error: %s\n", e.what()); }
    (void)gun_type;
}

void LuaManager::call_item_on_eject(int item_type, Entity& player) {
    const ItemDef* def = nullptr;
    for (auto const& d : items_)
        if (d.type == item_type) {
            def = &d;
            break;
        }
    if (!def || !def->on_eject.valid())
        return;
    LuaCtxGuard _ctx(ss, &player);
    auto r = def->on_eject(); if (!r.valid()) { sol::error e = r; std::fprintf(stderr, "[lua] item on_eject error: %s\n", e.what()); }
    (void)0; (void)item_type;
}

void LuaManager::call_item_on_reload_start(int item_type, Entity& player) {
    const ItemDef* def = nullptr;
    for (auto const& d : items_)
        if (d.type == item_type) {
            def = &d;
            break;
        }
    if (!def || !def->on_reload_start.valid())
        return;
    LuaCtxGuard _ctx2(ss, &player);
    auto r = def->on_reload_start(); if (!r.valid()) { sol::error e = r; std::fprintf(stderr, "[lua] item on_reload_start error: %s\n", e.what()); }
    (void)0; (void)item_type;
}

void LuaManager::call_item_on_reload_finish(int item_type, Entity& player) {
    const ItemDef* def = nullptr;
    for (auto const& d : items_)
        if (d.type == item_type) {
            def = &d;
            break;
        }
    if (!def || !def->on_reload_finish.valid())
        return;
    LuaCtxGuard _ctx3(ss, &player);
    auto r = def->on_reload_finish(); if (!r.valid()) { sol::error e = r; std::fprintf(stderr, "[lua] item on_reload_finish error: %s\n", e.what()); }
    (void)0;
}

void LuaManager::call_gun_on_active_reload(int gun_type, Entity& player) {
    (void)player;
    const GunDef* gd = find_gun_def_by_type(guns_, gun_type);
    if (!gd || !gd->on_active_reload.valid()) return;
    auto r = gd->on_active_reload(); if (!r.valid()) { sol::error e = r; std::fprintf(stderr, "[lua] on_active_reload gun error: %s\n", e.what()); }
}

void LuaManager::call_gun_on_failed_active_reload(int gun_type, Entity& player) {
    (void)player;
    const GunDef* gd = find_gun_def_by_type(guns_, gun_type);
    if (!gd || !gd->on_failed_active_reload.valid()) return;
    auto r = gd->on_failed_active_reload(); if (!r.valid()) { sol::error e = r; std::fprintf(stderr, "[lua] on_failed_active_reload gun error: %s\n", e.what()); }
}

void LuaManager::call_gun_on_tried_after_failed_ar(int gun_type, Entity& player) {
    (void)player;
    const GunDef* gd = find_gun_def_by_type(guns_, gun_type);
    if (!gd || !gd->on_tried_after_failed_ar.valid()) return;
    auto r = gd->on_tried_after_failed_ar(); if (!r.valid()) { sol::error e = r; std::fprintf(stderr, "[lua] on_tried_to_active_reload_after_failing gun error: %s\n", e.what()); }
}

void LuaManager::call_gun_on_step(int gun_type, Entity& player) {
    (void)player;
    const GunDef* gd = find_gun_def_by_type(guns_, gun_type);
    if (!gd || !gd->on_step.valid()) return;
    auto r = gd->on_step(); if (!r.valid()) { sol::error e = r; std::fprintf(stderr, "[lua] gun on_step error: %s\n", e.what()); }
}

void LuaManager::call_item_on_active_reload(int item_type, Entity& player) {
    const ItemDef* def = nullptr; for (auto const& d : items_) if (d.type == item_type) { def = &d; break; }
    if (!def || !def->on_active_reload.valid()) return;
    LuaCtxGuard _ctx(ss, &player);
    auto r = def->on_active_reload(); if (!r.valid()) { sol::error e = r; std::fprintf(stderr, "[lua] item on_active_reload error: %s\n", e.what()); }
    (void)0;
}

void LuaManager::call_item_on_failed_active_reload(int item_type, Entity& player) {
    const ItemDef* def = nullptr; for (auto const& d : items_) if (d.type == item_type) { def = &d; break; }
    if (!def || !def->on_failed_active_reload.valid()) return;
    LuaCtxGuard _ctx2(ss, &player);
    auto r = def->on_failed_active_reload(); if (!r.valid()) { sol::error e = r; std::fprintf(stderr, "[lua] item on_failed_active_reload error: %s\n", e.what()); }
    (void)0;
}

void LuaManager::call_item_on_tried_after_failed_ar(int item_type, Entity& player) {
    const ItemDef* def = nullptr; for (auto const& d : items_) if (d.type == item_type) { def = &d; break; }
    if (!def || !def->on_tried_after_failed_ar.valid()) return;
    LuaCtxGuard _ctx3(ss, &player);
    auto r = def->on_tried_after_failed_ar(); if (!r.valid()) { sol::error e = r; std::fprintf(stderr, "[lua] item on_tried_to_active_reload_after_failing error: %s\n", e.what()); }
    (void)0;
}

void LuaManager::call_gun_on_pickup(int gun_type, Entity& player) {
    (void)player;
    const GunDef* gd = find_gun_def_by_type(guns_, gun_type);
    if (!gd || !gd->on_pickup.valid()) return;
    auto r = gd->on_pickup(); if (!r.valid()) { sol::error e = r; std::fprintf(stderr, "[lua] gun on_pickup error: %s\n", e.what()); }
}

void LuaManager::call_gun_on_drop(int gun_type, Entity& player) {
    (void)player;
    const GunDef* gd = find_gun_def_by_type(guns_, gun_type);
    if (!gd || !gd->on_drop.valid()) return;
    auto r = gd->on_drop(); if (!r.valid()) { sol::error e = r; std::fprintf(stderr, "[lua] gun on_drop error: %s\n", e.what()); }
}

void LuaManager::call_item_on_pickup(int item_type, Entity& player) {
    const ItemDef* def = nullptr; for (auto const& d : items_) if (d.type == item_type) { def = &d; break; }
    if (!def || !def->on_pickup.valid()) return;
    LuaCtxGuard _ctx4(ss, &player);
    auto r = def->on_pickup(); if (!r.valid()) { sol::error e = r; std::fprintf(stderr, "[lua] item on_pickup error: %s\n", e.what()); }
    (void)0;
}

void LuaManager::call_item_on_drop(int item_type, Entity& player) {
    const ItemDef* def = nullptr; for (auto const& d : items_) if (d.type == item_type) { def = &d; break; }
    if (!def || !def->on_drop.valid()) return;
    LuaCtxGuard _ctx5(ss, &player);
    auto r = def->on_drop(); if (!r.valid()) { sol::error e = r; std::fprintf(stderr, "[lua] item on_drop error: %s\n", e.what()); }
    (void)0;
}

void LuaManager::call_generate_room() {
    sol::object obj = S->get<sol::object>("generate_room");
    if (!obj.is<sol::function>()) return;
    LuaCtxGuard _ctx(ss, nullptr);
    {
        auto r = obj.as<sol::protected_function>()();
        if (!r.valid()) { sol::error e = r; std::fprintf(stderr, "[lua] error in generate_room: %s\n", e.what()); }
    }
}

void LuaManager::call_gun_on_jam(int gun_type, Entity& player) {
    (void)player;
    const GunDef* gd = find_gun_def_by_type(guns_, gun_type);
    if (!gd || !gd->on_jam.valid()) return;
    auto r = gd->on_jam(); if (!r.valid()) { sol::error e = r; std::fprintf(stderr, "[lua] on_jam error: %s\n", e.what()); }
}

bool LuaManager::load_mods() {
    auto mods_root = mm->root;
    clear();
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
    return true;
}

bool LuaManager::call_item_on_use(int item_type, Entity& player,
                                  std::string* out_msg) {
    const ItemDef* def = nullptr;
    for (auto const& d : items_)
        if (d.type == item_type) {
            def = &d;
            break;
        }
    if (!def || !def->on_use.valid())
        return false;
    LuaCtxGuard _ctx(ss, &player);
    auto r = def->on_use();
    if (!r.valid()) { sol::error e = r; std::fprintf(stderr, "[lua] on_use error: %s\n", e.what()); g_state_ctx=nullptr; g_player_ctx=nullptr; return false; }
    if (out_msg && r.return_count() >= 1) {
        sol::object o = r.get<sol::object>();
        if (o.is<std::string>()) *out_msg = o.as<std::string>();
    }
    (void)0;
    return true;
}

void LuaManager::call_item_on_tick(int item_type, Entity& player, float dt) {
    const ItemDef* def = nullptr;
    for (auto const& d : items_)
        if (d.type == item_type) {
            def = &d;
            break;
        }
    if (!def || !def->on_tick.valid())
        return;
    LuaCtxGuard _ctx2(ss, &player);
    auto r = def->on_tick(dt);
    if (!r.valid()) { sol::error e = r; std::fprintf(stderr, "[lua] on_tick error: %s\n", e.what()); }
    (void)0;
}

void LuaManager::call_item_on_shoot(int item_type, Entity& player) {
    const ItemDef* def = nullptr;
    for (auto const& d : items_)
        if (d.type == item_type) {
            def = &d;
            break;
        }
    if (!def || !def->on_shoot.valid())
        return;
    LuaCtxGuard _ctx3(ss, &player);
    auto r = def->on_shoot(); if (!r.valid()) { sol::error e = r; std::fprintf(stderr, "[lua] on_shoot error: %s\n", e.what()); }
    (void)0;
}

void LuaManager::call_item_on_damage(int item_type, Entity& player, int attacker_ap) {
    const ItemDef* def = nullptr;
    for (auto const& d : items_)
        if (d.type == item_type) {
            def = &d;
            break;
        }
    if (!def || !def->on_damage.valid())
        return;
    {
        LuaCtxGuard _ctx(ss, &player);
        auto r = def->on_damage(attacker_ap);
        if (!r.valid()) { sol::error e = r; std::fprintf(stderr, "[lua] on_damage error: %s\n", e.what()); }
    }
}
