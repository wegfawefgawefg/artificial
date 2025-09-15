#include "luamgr.hpp"
// api: player
#include "lua/bindings.hpp"
#include "lua/lua_helpers.hpp"
#include "globals.hpp"

void lua_register_api_player(sol::state& s, LuaManager& m) {
    (void)m;
    auto api = s.create_named_table("api");
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

    // Dash helpers
    api.set_function("set_dash_max", [](int n) {
        if (g_state_ctx) {
            if (n < 0) n = 0;
            g_state_ctx->dash_max = n;
            if (g_state_ctx->dash_stocks > g_state_ctx->dash_max)
                g_state_ctx->dash_stocks = g_state_ctx->dash_max;
        }
    });
    api.set_function("set_dash_stocks", [](int n) {
        if (g_state_ctx) {
            if (n < 0) n = 0;
            if (n > g_state_ctx->dash_max) n = g_state_ctx->dash_max;
            g_state_ctx->dash_stocks = n;
        }
    });
    api.set_function("add_dash_stocks", [](int n) {
        if (g_state_ctx) {
            int v = g_state_ctx->dash_stocks + n;
            if (v < 0) v = 0;
            if (v > g_state_ctx->dash_max) v = g_state_ctx->dash_max;
            g_state_ctx->dash_stocks = v;
        }
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
}
