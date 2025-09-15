// Crate progression implementation (open progress and loot drops).
#include "globals.hpp"
#include "state.hpp"
#include "luamgr.hpp"
#include "items.hpp"
#include "guns.hpp"
#include "graphics.hpp"

#include <algorithm>
#include <random>

void update_crates_open() {
    if (!ss || !ss->player_vid) return;
    const Entity* p = ss->entities.get(*ss->player_vid);
    if (!p) return;
    glm::vec2 ph = p->half_size();
    float pl = p->pos.x - ph.x, pr = p->pos.x + ph.x;
    float pt = p->pos.y - ph.y, pb = p->pos.y + ph.y;
    for (auto& c : ss->crates.data()) if (c.active && !c.opened) {
        glm::vec2 ch = c.size * 0.5f;
        float cl = c.pos.x - ch.x, cr = c.pos.x + ch.x;
        float ct = c.pos.y - ch.y, cb = c.pos.y + ch.y;
        bool overlap = !(pr <= cl || pl >= cr || pb <= ct || pt >= cb);
        float open_time = 5.0f;
        if (luam) if (auto const* cd = luam->find_crate(c.def_type)) open_time = cd->open_time;
        if (overlap) c.open_progress = std::min(open_time, c.open_progress + TIMESTEP);
        else c.open_progress = std::max(0.0f, c.open_progress - TIMESTEP * 0.5f);
        if (c.open_progress >= open_time) {
            c.opened = true; c.active = false; ss->metrics.crates_opened += 1;
            glm::vec2 pos = c.pos;
            if (luam) {
                DropTables dt{}; bool have = false;
                if (auto const* cd = luam->find_crate(c.def_type)) { dt = cd->drops; have = true; }
                if (!have) dt = luam->drops();
                static thread_local std::mt19937 rng{std::random_device{}()};
                std::uniform_real_distribution<float> U(0.0f, 1.0f);
                auto pick_weighted = [&](const std::vector<DropEntry>& v) -> int {
                    if (v.empty()) return -1;
                    float sum = 0.0f;
                    for (auto const& de : v) sum += de.weight;
                    if (sum <= 0.0f) return -1;
                    std::uniform_real_distribution<float> du(0.0f, sum);
                    float r = du(rng), acc = 0.0f;
                    for (auto const& de : v) { acc += de.weight; if (r <= acc) return de.type; }
                    return v.back().type;
                };
                float cval = U(rng);
                if (cval < 0.6f && !dt.items.empty()) {
                    int t = pick_weighted(dt.items);
                    if (t >= 0) {
                        auto it = std::find_if(luam->items().begin(), luam->items().end(), [&](const ItemDef& d) { return d.type == t; });
                        if (it != luam->items().end()) {
                            if (auto iv = ss->items.spawn_from_def(*it, 1)) {
                                ss->ground_items.spawn(*iv, pos);
                                ss->metrics.items_spawned += 1;
                            }
                        }
                    }
                } else if (!dt.guns.empty()) {
                    int t = pick_weighted(dt.guns);
                    if (t >= 0) {
                        auto ig = std::find_if(luam->guns().begin(), luam->guns().end(), [&](const GunDef& g) { return g.type == t; });
                        if (ig != luam->guns().end()) {
                            if (auto gv = ss->guns.spawn_from_def(*ig)) {
                                int sid = -1;
                                if (!ig->sprite.empty() && ig->sprite.find(':') != std::string::npos)
                                    sid = try_get_sprite_id(ig->sprite);
                                ss->ground_guns.spawn(*gv, pos, sid);
                                ss->metrics.guns_spawned += 1;
                            }
                        }
                    }
                }
                if (ss->player_vid) if (auto* plent = ss->entities.get_mut(*ss->player_vid)) luam->call_crate_on_open(c.def_type, *plent);
            }
        }
    }
}
