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
    // Reset world
    ss->gun_cooldown = 0.0f;
    ss->projectiles = Projectiles{};
    ss->entities = Entities{};
    ss->player_vid.reset();
    ss->start_tile = {-1, -1};
    ss->exit_tile = {-1, -1};
    ss->exit_countdown = -1.0f;
    ss->score_ready_timer = 0.0f;
    ss->pickups.clear();
    ss->ground_items.clear();
    // Rebuild stage
    // Random dimensions between 32 and 64
    std::mt19937 rng{std::random_device{}()};
    std::uniform_int_distribution<int> dwh(32, 64);
    uint32_t W = static_cast<uint32_t>(dwh(rng));
    uint32_t H = static_cast<uint32_t>(dwh(rng));
    ss->stage = Stage(W, H);
    // Reset per-stage metrics for a fresh room
    ss->metrics.reset(Entities::MAX);
    ss->stage.fill_border(TileProps::Make(true, true));
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
        if (ss->stage.in_bounds(c.x, c.y) && !ss->stage.at(c.x, c.y).blocks_entities()) {
            start_idx = i;
            break;
        }
    }
    for (int i = (int)corners.size() - 1; i >= 0; --i) {
        if (i == start_idx)
            continue;
        auto c = corners[static_cast<size_t>(i)];
        if (ss->stage.in_bounds(c.x, c.y) && !ss->stage.at(c.x, c.y).blocks_entities()) {
            exit_idx = i;
            break;
        }
    }
    if (start_idx < 0) {
        start_idx = 0;
        auto c = corners[static_cast<size_t>(0)];
        ss->stage.at(c.x, c.y) = TileProps::Make(false, false);
    }
    if (exit_idx < 0 || exit_idx == start_idx) {
        exit_idx = (start_idx + 3) % 4;
        auto c = corners[static_cast<size_t>(exit_idx)];
        ss->stage.at(c.x, c.y) = TileProps::Make(false, false);
    }

    ss->start_tile = corners[static_cast<size_t>(start_idx)];
    ss->exit_tile = corners[static_cast<size_t>(exit_idx)];
    // Place obstacles now, avoiding start/exit tiles
    for (int i = 0; i < obstacles; ++i) {
        int x = dx(rng);
        int y = dy(rng);
        if ((x == ss->start_tile.x && y == ss->start_tile.y) ||
            (x == ss->exit_tile.x && y == ss->exit_tile.y)) {
            continue;
        }
        int t = type(rng);
        if (t <= 1) {
            ss->stage.at(x, y) = TileProps::Make(true, false); // void/water: blocks entities only
        } else {
            ss->stage.at(x, y) = TileProps::Make(true, true); // wall: blocks both
        }
    }

    // Create player at start
    if (auto pvid = ss->entities.new_entity()) {
        Entity* p = ss->entities.get_mut(*pvid);
        p->type_ = ids::ET_PLAYER;
        p->size = {0.125f, 0.125f};
        p->sprite_size = {0.25f, 0.25f};
        p->pos = {static_cast<float>(ss->start_tile.x) + 0.5f,
                  static_cast<float>(ss->start_tile.y) + 0.5f};
        p->sprite_id = try_get_sprite_id("base:player");
        p->max_hp = 1000;
        p->health = p->max_hp;
        p->shield = p->stats.shield_max;
        ss->player_vid = pvid;
    }

    // NPCs are now spawned via Lua using api.spawn_entity_safe; if no entity defs exist, skip.

    // Camera to player and zoom for ~8%
    if (ss->player_vid) {
        int ww = 0, wh = 0;
        SDL_GetRendererOutputSize(gg->renderer, &ww, &wh);
        float min_dim = static_cast<float>(std::min(ww, wh));
        const Entity* p = ss->entities.get(*ss->player_vid);
        if (p) {
            float desired_px = 0.08f * min_dim;
            float zoom = desired_px / (p->size.y * TILE_SIZE);
            zoom = std::clamp(zoom, 0.5f, 32.0f);
            gg->play_cam.zoom = zoom;
            gg->play_cam.pos = p->pos;
        }
    }

    // Spawn a few Lua-defined pickups/items near start for testing
    {
        glm::vec2 base = {static_cast<float>(ss->start_tile.x) + 0.5f,
                          static_cast<float>(ss->start_tile.y) + 0.5f};
        auto place = [&](glm::vec2 offs) { return ensure_not_in_block(base + offs); };
        if (luam && !luam->powerups().empty()) {
            auto& pu = luam->powerups()[0];
            auto* p = ss->pickups.spawn(static_cast<uint32_t>(pu.type), pu.name,
                                          place({1.0f, 0.0f}));
            if (p) {
                if (!pu.sprite.empty() && pu.sprite.find(':') != std::string::npos)
                    p->sprite_id = try_get_sprite_id(pu.sprite);
                else
                    p->sprite_id = -1;
            }
        }
        if (luam && luam->powerups().size() > 1) {
            auto& pu = luam->powerups()[1];
            auto* p = ss->pickups.spawn(static_cast<uint32_t>(pu.type), pu.name,
                                          place({0.0f, 1.0f}));
            if (p) {
                if (!pu.sprite.empty() && pu.sprite.find(':') != std::string::npos)
                    p->sprite_id = try_get_sprite_id(pu.sprite);
                else
                    p->sprite_id = -1;
            }
        }
        // Place example guns near spawn: pistol and rifle if present in Lua
        if (luam && !luam->guns().empty()) {
            auto gd = luam->guns()[0];
            auto gv = ss->guns.spawn_from_def(gd);
            int sid = -1;
            if (!gd.sprite.empty() && gd.sprite.find(':') != std::string::npos)
                sid = try_get_sprite_id(gd.sprite);
            if (gv)
                ss->ground_guns.spawn(*gv, place({2.0f, 0.0f}), sid);
        }
        if (luam && luam->guns().size() > 1) {
            auto gd = luam->guns()[1];
            auto gv = ss->guns.spawn_from_def(gd);
            int sid = -1;
            if (!gd.sprite.empty() && gd.sprite.find(':') != std::string::npos)
                sid = try_get_sprite_id(gd.sprite);
            if (gv)
                ss->ground_guns.spawn(*gv, place({0.0f, 2.0f}), sid);
        }
        // TEMP: spawn player with three shotguns for testing
        if (luam && ss->player_vid) {
            Entity* p = ss->entities.get_mut(*ss->player_vid);
            auto add_gun_to_inv = [&](int gun_type) {
                for (auto const& g : luam->guns()) {
                    if (g.type == gun_type) {
                        if (auto gv = ss->guns.spawn_from_def(g)) {
                            if (ss->player_vid) if (auto* inv = ss->inv_for(*ss->player_vid)) inv->insert_existing(INV_GUN, *gv);
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
        if (luam)
            luam->call_generate_room();
    }
}

bool tile_blocks_entity(int x, int y) {
    return !ss->stage.in_bounds(x, y) || ss->stage.at(x, y).blocks_entities();
}

glm::ivec2 nearest_walkable_tile(glm::ivec2 t, int max_radius) {
    if (!tile_blocks_entity(t.x, t.y))
        return t;
    for (int r = 1; r <= max_radius; ++r) {
        for (int dy = -r; dy <= r; ++dy) {
            int y = t.y + dy;
            int dx = r - std::abs(dy);
            for (int sx : {-dx, dx}) {
                int x = t.x + sx;
                if (!tile_blocks_entity(x, y))
                    return {x, y};
            }
        }
    }
    return t;
}

glm::vec2 ensure_not_in_block(glm::vec2 pos) {
    glm::ivec2 t = {static_cast<int>(std::floor(pos.x)), static_cast<int>(std::floor(pos.y))};
    glm::ivec2 w = nearest_walkable_tile(t, 16);
    if (w != t)
        return glm::vec2{(float)w.x + 0.5f, (float)w.y + 0.5f};
    return pos;
}
