#include "room.hpp"
#include "globals.hpp"

#include "globals.hpp"
#include "luamgr.hpp"
#include "sprites.hpp"

#include <SDL2/SDL.h>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <random>

void generate_room() {
    auto& state = *g_state;
    auto& gfx = *g_gfx;
    // Reset world
    state.projectiles = Projectiles{};
    state.entities = Entities{};
    state.player_vid.reset();
    state.start_tile = {-1, -1};
    state.exit_tile = {-1, -1};
    state.exit_countdown = -1.0f;
    state.score_ready_timer = 0.0f;
    state.pickups.clear();
    state.ground_items.clear();
    // Rebuild stage
    // Random dimensions between 32 and 64
    std::mt19937 rng{std::random_device{}()};
    std::uniform_int_distribution<int> dwh(32, 64);
    std::uint32_t W = static_cast<std::uint32_t>(dwh(rng));
    std::uint32_t H = static_cast<std::uint32_t>(dwh(rng));
    state.stage = Stage(W, H);
    // Reset per-stage metrics for a fresh room
    state.metrics.reset(Entities::MAX);
    state.stage.fill_border(TileProps::Make(true, true));
    // sprinkle obstacles (walls and voids)
    int tiles = static_cast<int>(W * H);
    int obstacles = tiles / 8; // ~12.5%
    std::uniform_int_distribution<int> dx(1, static_cast<int>(W) - 2);
    std::uniform_int_distribution<int> dy(1, static_cast<int>(H) - 2);
    std::uniform_int_distribution<int> type(0, 3); // 0..1 void (blocks entities only), 2..3 wall (blocks both)

    // Determine start and exit corners (inside border)
    std::vector<glm::ivec2> corners = {{1, 1},
                                       {static_cast<int>(W) - 2, 1},
                                       {1, static_cast<int>(H) - 2},
                                       {static_cast<int>(W) - 2, static_cast<int>(H) - 2}};
    // Pick first two distinct non-block corners
    int start_idx = -1, exit_idx = -1;
    for (int i = 0; i < (int)corners.size(); ++i) {
        auto c = corners[static_cast<size_t>(i)];
        if (state.stage.in_bounds(c.x, c.y) && !state.stage.at(c.x, c.y).blocks_entities()) {
            start_idx = i;
            break;
        }
    }
    for (int i = (int)corners.size() - 1; i >= 0; --i) {
        if (i == start_idx)
            continue;
        auto c = corners[static_cast<size_t>(i)];
        if (state.stage.in_bounds(c.x, c.y) && !state.stage.at(c.x, c.y).blocks_entities()) {
            exit_idx = i;
            break;
        }
    }
    if (start_idx < 0) {
        start_idx = 0;
        auto c = corners[static_cast<size_t>(0)];
        state.stage.at(c.x, c.y) = TileProps::Make(false, false);
    }
    if (exit_idx < 0 || exit_idx == start_idx) {
        exit_idx = (start_idx + 3) % 4;
        auto c = corners[static_cast<size_t>(exit_idx)];
        state.stage.at(c.x, c.y) = TileProps::Make(false, false);
    }

    state.start_tile = corners[static_cast<size_t>(start_idx)];
    state.exit_tile = corners[static_cast<size_t>(exit_idx)];
    // Place obstacles now, avoiding start/exit tiles
    for (int i = 0; i < obstacles; ++i) {
        int x = dx(rng);
        int y = dy(rng);
        if ((x == state.start_tile.x && y == state.start_tile.y) ||
            (x == state.exit_tile.x && y == state.exit_tile.y)) {
            continue;
        }
        int t = type(rng);
        if (t <= 1) {
            state.stage.at(x, y) = TileProps::Make(true, false); // void/water: blocks entities only
        } else {
            state.stage.at(x, y) = TileProps::Make(true, true); // wall: blocks both
        }
    }

    // Create player at start
    if (auto pvid = state.entities.new_entity()) {
        Entity* p = state.entities.get_mut(*pvid);
        p->type_ = ids::ET_PLAYER;
        p->size = {0.25f, 0.25f};
        p->pos = {static_cast<float>(state.start_tile.x) + 0.5f,
                  static_cast<float>(state.start_tile.y) + 0.5f};
        if (g_sprite_ids)
            p->sprite_id = g_sprite_ids->try_get("base:player");
        p->max_hp = 1000;
        p->health = p->max_hp;
        p->shield = p->stats.shield_max;
        state.player_vid = pvid;
    }

    // Spawn some NPCs
    {
        std::mt19937 rng2{std::random_device{}()};
        std::uniform_int_distribution<int> dx2(1, (int)state.stage.get_width() - 2);
        std::uniform_int_distribution<int> dy2(1, (int)state.stage.get_height() - 2);
        for (int i = 0; i < 25; ++i) {
            auto vid = state.entities.new_entity();
            if (!vid)
                break;
            Entity* e = state.entities.get_mut(*vid);
            e->type_ = ids::ET_NPC;
            e->size = {0.25f, 0.25f};
            if (g_sprite_ids)
                e->sprite_id = g_sprite_ids->try_get("base:zombie");
            // Beef up NPC durability for clearer damage visualization
            e->max_hp = 2000;
            e->health = e->max_hp;
            e->stats.shield_max = 500.0f;
            e->shield = e->stats.shield_max;
            e->stats.plates = 5;
            for (int tries = 0; tries < 100; ++tries) {
                int x = dx2(rng2), y = dy2(rng2);
                if (!state.stage.at(x, y).blocks_entities()) {
                    e->pos = {static_cast<float>(x) + 0.5f, static_cast<float>(y) + 0.5f};
                    break;
                }
            }
        }
    }

    // Camera to player and zoom for ~8%
    if (gfx.renderer && state.player_vid) {
        int ww = 0, wh = 0;
        SDL_GetRendererOutputSize(gfx.renderer, &ww, &wh);
        float min_dim = static_cast<float>(std::min(ww, wh));
        const Entity* p = state.entities.get(*state.player_vid);
        if (p) {
            float desired_px = 0.08f * min_dim;
            float zoom = desired_px / (p->size.y * TILE_SIZE);
            zoom = std::clamp(zoom, 0.5f, 32.0f);
            gfx.play_cam.zoom = zoom;
            gfx.play_cam.pos = p->pos;
        }
    }

    // Spawn a few Lua-defined pickups/items near start for testing
    {
        glm::vec2 base = {static_cast<float>(state.start_tile.x) + 0.5f,
                          static_cast<float>(state.start_tile.y) + 0.5f};
        auto place = [&](glm::vec2 offs) { return ensure_not_in_block(state, base + offs); };
        if (g_lua_mgr && !g_lua_mgr->powerups().empty()) {
            auto& pu = g_lua_mgr->powerups()[0];
            auto* p = state.pickups.spawn(static_cast<std::uint32_t>(pu.type), pu.name,
                                          place({1.0f, 0.0f}));
            if (p && g_sprite_ids) {
                if (!pu.sprite.empty() && pu.sprite.find(':') != std::string::npos)
                    p->sprite_id = g_sprite_ids->try_get(pu.sprite);
                else
                    p->sprite_id = -1;
            }
        }
        if (g_lua_mgr && g_lua_mgr->powerups().size() > 1) {
            auto& pu = g_lua_mgr->powerups()[1];
            auto* p = state.pickups.spawn(static_cast<std::uint32_t>(pu.type), pu.name,
                                          place({0.0f, 1.0f}));
            if (p && g_sprite_ids) {
                if (!pu.sprite.empty() && pu.sprite.find(':') != std::string::npos)
                    p->sprite_id = g_sprite_ids->try_get(pu.sprite);
                else
                    p->sprite_id = -1;
            }
        }
        // Place example guns near spawn: pistol and rifle if present in Lua
        if (g_lua_mgr && !g_lua_mgr->guns().empty()) {
            auto gd = g_lua_mgr->guns()[0];
            auto gv = state.guns.spawn_from_def(gd);
            int sid = -1;
            if (g_sprite_ids) {
                if (!gd.sprite.empty() && gd.sprite.find(':') != std::string::npos)
                    sid = g_sprite_ids->try_get(gd.sprite);
            }
            if (gv)
                state.ground_guns.spawn(*gv, place({2.0f, 0.0f}), sid);
        }
        if (g_lua_mgr && g_lua_mgr->guns().size() > 1) {
            auto gd = g_lua_mgr->guns()[1];
            auto gv = state.guns.spawn_from_def(gd);
            int sid = -1;
            if (g_sprite_ids) {
                if (!gd.sprite.empty() && gd.sprite.find(':') != std::string::npos)
                    sid = g_sprite_ids->try_get(gd.sprite);
            }
            if (gv)
                state.ground_guns.spawn(*gv, place({0.0f, 2.0f}), sid);
        }
        // TEMP: spawn player with three shotguns for testing
        if (g_lua_mgr && state.player_vid) {
            Entity* p = state.entities.get_mut(*state.player_vid);
            auto add_gun_to_inv = [&](int gun_type) {
                for (auto const& g : g_lua_mgr->guns()) {
                    if (g.type == gun_type) {
                        if (auto gv = state.guns.spawn_from_def(g)) {
                            state.inventory.insert_existing(INV_GUN, *gv);
                            return *gv;
                        }
                    }
                }
                return VID{};
            };
            VID v1{}, v2{}, v3{};
            v1 = add_gun_to_inv(210);
            v2 = add_gun_to_inv(211);
            v3 = add_gun_to_inv(212);
            if (v1.id || v2.id || v3.id) {
                // Equip the pump if available
                if (v1.id) p->equipped_gun_vid = v1;
                else if (v2.id) p->equipped_gun_vid = v2;
                else if (v3.id) p->equipped_gun_vid = v3;
            }
        }
        // Let Lua generate room content (crates, loot, etc.) if function is present
        if (g_lua_mgr)
            g_lua_mgr->call_generate_room(state);
    }
}

bool tile_blocks_entity(const State& state, int x, int y) {
    return !state.stage.in_bounds(x, y) || state.stage.at(x, y).blocks_entities();
}

glm::ivec2 nearest_walkable_tile(const State& state, glm::ivec2 t, int max_radius) {
    if (!tile_blocks_entity(state, t.x, t.y))
        return t;
    for (int r = 1; r <= max_radius; ++r) {
        for (int dy = -r; dy <= r; ++dy) {
            int y = t.y + dy;
            int dx = r - std::abs(dy);
            for (int sx : {-dx, dx}) {
                int x = t.x + sx;
                if (!tile_blocks_entity(state, x, y))
                    return {x, y};
            }
        }
    }
    return t;
}

glm::vec2 ensure_not_in_block(const State& state, glm::vec2 pos) {
    glm::ivec2 t = {static_cast<int>(std::floor(pos.x)), static_cast<int>(std::floor(pos.y))};
    glm::ivec2 w = nearest_walkable_tile(state, t, 16);
    if (w != t)
        return glm::vec2{(float)w.x + 0.5f, (float)w.y + 0.5f};
    return pos;
}
