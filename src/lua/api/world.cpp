#include "luamgr.hpp"
// api: world
#include "lua/bindings.hpp"
#include "lua/lua_helpers.hpp"
#include "globals.hpp"
#include "graphics.hpp"
#include <glm/glm.hpp>
#include <cmath>

void lua_register_api_world(sol::state& s, LuaManager& m) {
    (void)m;
    auto api = s.create_named_table("api");
    // World spawn helpers (require g_state_ctx)
    api.set_function("spawn_crate", [](int type, float x, float y) {
        if (!g_state_ctx) return;
        auto pos = glm::vec2{x, y};
        auto tile_blocks = [&](int tx, int ty) {
            return !g_state_ctx->stage.in_bounds(tx, ty) ||
                   g_state_ctx->stage.at(tx, ty).blocks_entities();
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
        glm::ivec2 t{(int)std::floor(pos.x), (int)std::floor(pos.y)};
        auto w = nearest(t.x, t.y);
        glm::vec2 safe{(float)w.x + 0.5f, (float)w.y + 0.5f};
        g_state_ctx->crates.spawn(safe, type);
        g_state_ctx->metrics.crates_spawned += 1;
    });

    api.set_function("spawn_crate_safe", [](int type, float x, float y) {
        if (!g_state_ctx) return;
        auto pos = glm::vec2{x, y};
        auto tile_blocks = [&](int tx, int ty) {
            return !g_state_ctx->stage.in_bounds(tx, ty) ||
                   g_state_ctx->stage.at(tx, ty).blocks_entities();
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
        glm::ivec2 t{(int)std::floor(pos.x), (int)std::floor(pos.y)};
        auto w = nearest(t.x, t.y);
        glm::vec2 safe{(float)w.x + 0.5f, (float)w.y + 0.5f};
        g_state_ctx->crates.spawn(safe, type);
        g_state_ctx->metrics.crates_spawned += 1;
    });

    api.set_function("spawn_item", [](int type, int count, float x, float y) {
        if (!g_state_ctx || !g_mgr) return;
        const ItemDef* id = nullptr;
        for (auto const& d : g_mgr->items()) if (d.type == type) { id = &d; break; }
        if (!id) return;
        auto iv = g_state_ctx->items.spawn_from_def(*id, (uint32_t)std::max(1, count));
        if (iv) {
            auto pos = glm::vec2{x, y};
            auto tile_blocks = [&](int tx, int ty) {
                return !g_state_ctx->stage.in_bounds(tx, ty) ||
                       g_state_ctx->stage.at(tx, ty).blocks_entities();
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
            glm::ivec2 t{(int)std::floor(pos.x), (int)std::floor(pos.y)};
            auto w = nearest(t.x, t.y);
            glm::vec2 safe{(float)w.x + 0.5f, (float)w.y + 0.5f};
            g_state_ctx->ground_items.spawn(*iv, safe);
            g_state_ctx->metrics.items_spawned += 1;
        }
    });

    api.set_function("spawn_gun", [](int type, float x, float y) {
        if (!g_state_ctx || !g_mgr) return;
        const GunDef* gd = nullptr;
        for (auto const& g : g_mgr->guns()) if (g.type == type) { gd = &g; break; }
        if (!gd) return;
        auto gv = g_state_ctx->guns.spawn_from_def(*gd);
        if (gv) {
            auto pos = glm::vec2{x, y};
            auto tile_blocks = [&](int tx, int ty) {
                return !g_state_ctx->stage.in_bounds(tx, ty) ||
                       g_state_ctx->stage.at(tx, ty).blocks_entities();
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
            glm::ivec2 t{(int)std::floor(pos.x), (int)std::floor(pos.y)};
            auto w = nearest(t.x, t.y);
            glm::vec2 safe{(float)w.x + 0.5f, (float)w.y + 0.5f};
            int sid = -1;
            g_state_ctx->ground_guns.spawn(*gv, safe, sid);
            g_state_ctx->metrics.guns_spawned += 1;
        }
    });

    // Spawn an entity by type at world coords (center). Safe placement.
    api.set_function("spawn_entity_safe", [](int type, float x, float y) {
        if (!g_state_ctx) return;
        const EntityTypeDef* ed = g_mgr ? g_mgr->find_entity_type(type) : nullptr;
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
        g_mgr->call_entity_on_spawn(type, *e);
    });

    // Alias: always safe; emits alert on failure
    api.set_function("spawn_entity", [](int type, float x, float y) {
        if (!g_state_ctx) return;
        const EntityTypeDef* ed = g_mgr ? g_mgr->find_entity_type(type) : nullptr;
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
        g_mgr->call_entity_on_spawn(type, *e);
    });
}
