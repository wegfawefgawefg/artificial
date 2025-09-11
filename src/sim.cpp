#include "sim.hpp"

#include "globals.hpp"
#include "luamgr.hpp"
#include "settings.hpp"
#include "room.hpp"
#include "sprites.hpp"
#include "guns.hpp"
#include "items.hpp"
#include "mods.hpp"
#include "sound.hpp"
#include <algorithm>
#include <cmath>
#include <random>

void sim_pre_physics_ticks(State& state) {
    if (state.player_vid && g_lua_mgr) {
        Entity* plbt = state.entities.get_mut(*state.player_vid);
        if (plbt) {
            const float dt = TIMESTEP;
            const int MAX_TICKS = 4000;
            int tick_calls = 0;
            // Guns with on_step
            for (const auto& entry : state.inventory.entries) {
                if (entry.kind != INV_GUN)
                    continue;
                GunInstance* gi = state.guns.get(entry.vid);
                if (!gi)
                    continue;
                const GunDef* gd = nullptr;
                for (auto const& g : g_lua_mgr->guns())
                    if (g.type == gi->def_type) {
                        gd = &g;
                        break;
                    }
                if (!gd || gd->on_step_ref < 0)
                    continue;
                if (gd->tick_rate_hz <= 0.0f || gd->tick_phase == "after")
                    continue;
                gi->tick_acc += dt;
                float period = 1.0f / std::max(1.0f, gd->tick_rate_hz);
                while (gi->tick_acc >= period && tick_calls < MAX_TICKS) {
                    g_lua_mgr->call_gun_on_step(gi->def_type, state, *plbt);
                    gi->tick_acc -= period;
                    ++tick_calls;
                }
            }
            // Items with on_tick
            for (const auto& entry : state.inventory.entries) {
                if (entry.kind != INV_ITEM)
                    continue;
                ItemInstance* inst = state.items.get(entry.vid);
                if (!inst)
                    continue;
                const ItemDef* idf = nullptr;
                for (auto const& d : g_lua_mgr->items())
                    if (d.type == inst->def_type) {
                        idf = &d;
                        break;
                    }
                if (!idf || idf->on_tick_ref < 0)
                    continue;
                if (idf->tick_rate_hz <= 0.0f || idf->tick_phase == "after")
                    continue;
                inst->tick_acc += dt;
                float period = 1.0f / std::max(1.0f, idf->tick_rate_hz);
                while (inst->tick_acc >= period && tick_calls < MAX_TICKS) {
                    g_lua_mgr->call_item_on_tick(inst->def_type, state, *plbt, period);
                    inst->tick_acc -= period;
                    ++tick_calls;
                }
            }
        }
    }
}

void sim_move_and_collide(State& state, Graphics& gfx) {
    (void)gfx; // currently unused here, reserved for future camera/scale interactions
    for (auto& e : state.entities.data()) {
        if (!e.active)
            continue;
        e.time_since_damage += TIMESTEP;
        if (e.type_ == ids::ET_PLAYER) {
            glm::vec2 dir{0.0f, 0.0f};
            if (state.playing_inputs.left)
                dir.x -= 1.0f;
            if (state.playing_inputs.right)
                dir.x += 1.0f;
            if (state.playing_inputs.up)
                dir.y -= 1.0f;
            if (state.playing_inputs.down)
                dir.y += 1.0f;
            if (dir.x != 0.0f || dir.y != 0.0f)
                dir = glm::normalize(dir);
            float scale = (e.stats.move_speed > 0.0f) ? (e.stats.move_speed / 350.0f) : 1.0f;
            state.dash_timer = std::max(0.0f, state.dash_timer - TIMESTEP);
            if (state.dash_stocks < state.dash_max) {
                state.dash_refill_timer += TIMESTEP;
                while (state.dash_refill_timer >= DASH_COOLDOWN_SECONDS && state.dash_stocks < state.dash_max) {
                    state.dash_refill_timer -= DASH_COOLDOWN_SECONDS;
                    state.dash_stocks += 1;
                }
            } else {
                state.dash_refill_timer = 0.0f;
            }
            static bool prev_dash = false;
            bool now_dash = state.playing_inputs.dash;
            if (now_dash && !prev_dash && state.dash_stocks > 0) {
                if (dir.x != 0.0f || dir.y != 0.0f) {
                    state.dash_dir = dir;
                    state.dash_timer = DASH_TIME_SECONDS;
                    state.dash_stocks -= 1;
                    if (state.player_vid) {
                        if (auto* pm = state.metrics_for(*state.player_vid)) {
                            pm->dashes_used += 1;
                            pm->dash_distance += DASH_SPEED_UNITS_PER_SEC * DASH_TIME_SECONDS;
                        }
                    }
                    state.reticle_shake = std::max(state.reticle_shake, 8.0f);
                    if (g_lua_mgr && state.player_vid) {
                        if (auto* pl = state.entities.get_mut(*state.player_vid))
                            g_lua_mgr->call_on_dash(state, *pl);
                    }
                }
            }
            prev_dash = now_dash;
            // Movement inaccuracy accumulator
            {
                float spd = std::sqrt(e.vel.x * e.vel.x + e.vel.y * e.vel.y);
                float factor = std::clamp(spd / PLAYER_SPEED_UNITS_PER_SEC, 0.0f, 4.0f);
                if (factor > 0.01f) {
                    e.move_spread_deg = std::min(e.stats.move_spread_max_deg,
                        e.move_spread_deg + e.stats.move_spread_inc_rate_deg_per_sec_at_base * factor * TIMESTEP);
                } else {
                    e.move_spread_deg = std::max(0.0f,
                        e.move_spread_deg - e.stats.move_spread_decay_deg_per_sec * TIMESTEP);
                }
            }
            if (state.dash_timer > 0.0f) {
                e.vel = state.dash_dir * DASH_SPEED_UNITS_PER_SEC;
            } else {
                e.vel = dir * (PLAYER_SPEED_UNITS_PER_SEC * scale);
            }
        } else {
            // NPC random drift
            if (e.rot <= 0.0f) {
                static thread_local std::mt19937 rng{std::random_device{}()};
                std::uniform_int_distribution<int> dirD(0, 4);
                std::uniform_real_distribution<float> dur(0.5f, 2.0f);
                int dir = dirD(rng);
                glm::vec2 v{0, 0};
                if (dir == 0) v = {-1, 0};
                else if (dir == 1) v = {1, 0};
                else if (dir == 2) v = {0, -1};
                else if (dir == 3) v = {0, 1};
                e.vel = v * 2.0f;
                e.rot = dur(rng);
            } else {
                e.rot -= TIMESTEP;
            }
        }
        int steps = std::max(1, e.physics_steps);
        glm::vec2 step_dpos = e.vel * (TIMESTEP / static_cast<float>(steps));
        for (int s = 0; s < steps; ++s) {
            // X axis
            float next_x = e.pos.x + step_dpos.x;
            {
                glm::vec2 half = e.half_size();
                glm::vec2 tl = {next_x - half.x, e.pos.y - half.y};
                glm::vec2 br = {next_x + half.x, e.pos.y + half.y};
                int minx = (int)floorf(tl.x), miny = (int)floorf(tl.y), maxx = (int)floorf(br.x), maxy = (int)floorf(br.y);
                bool blocked = false;
                for (int y = miny; y <= maxy && !blocked; ++y)
                    for (int x = minx; x <= maxx; ++x)
                        if (state.stage.in_bounds(x, y) && state.stage.at(x, y).blocks_entities()) {
                            blocked = true;
                        }
                if (!blocked) e.pos.x = next_x; else e.vel.x = 0.0f;
            }
            // Y axis
            float next_y = e.pos.y + step_dpos.y;
            {
                glm::vec2 half = e.half_size();
                glm::vec2 tl = {e.pos.x - half.x, next_y - half.y};
                glm::vec2 br = {e.pos.x + half.x, next_y + half.y};
                int minx = (int)floorf(tl.x), miny = (int)floorf(tl.y), maxx = (int)floorf(br.x), maxy = (int)floorf(br.y);
                bool blocked = false;
                for (int y = miny; y <= maxy && !blocked; ++y)
                    for (int x = minx; x <= maxx; ++x)
                        if (state.stage.in_bounds(x, y) && state.stage.at(x, y).blocks_entities()) {
                            blocked = true;
                        }
                if (!blocked) e.pos.y = next_y; else e.vel.y = 0.0f;
            }
        }
    }
}

void sim_shield_and_reload(State& state) {
    for (auto& e : state.entities.data()) {
        if (!e.active)
            continue;
        if (e.stats.shield_max > 0.0f && e.time_since_damage >= 3.0f) {
            e.shield = std::min(e.stats.shield_max, e.shield + e.stats.shield_regen * TIMESTEP);
        }
        if (e.type_ == ids::ET_PLAYER && e.equipped_gun_vid.has_value()) {
            if (auto* gi = state.guns.get(*e.equipped_gun_vid)) {
                if (gi->reloading) {
                    const GunDef* gd = nullptr;
                    if (g_lua_mgr) {
                        for (auto const& g : g_lua_mgr->guns())
                            if (g.type == gi->def_type) { gd = &g; break; }
                    }
                    if (gi->reload_eject_remaining > 0.0f) {
                        gi->reload_eject_remaining = std::max(0.0f, gi->reload_eject_remaining - TIMESTEP);
                    } else if (gi->reload_total_time > 0.0f) {
                        gi->reload_progress = std::min(1.0f, gi->reload_progress + (TIMESTEP / gi->reload_total_time));
                    }
                    if (gi->reload_progress >= 1.0f) {
                        if (gd && gi->ammo_reserve > 0) {
                            int take = std::min(gd->mag, gi->ammo_reserve);
                            gi->current_mag = take;
                            gi->ammo_reserve -= take;
                        }
                        gi->reloading = false;
                        gi->reload_progress = 0.0f;
                        gi->burst_remaining = 0;
                        gi->burst_timer = 0.0f;
                    }
                }
            }
        }
    }
}

void sim_toggle_drop_mode(State& state) {
    static bool prev_drop = false;
    if (state.playing_inputs.drop && !prev_drop) {
        state.drop_mode = !state.drop_mode;
        state.alerts.push_back({state.drop_mode ? std::string("Drop mode: press 1–0 to drop")
                                               : std::string("Drop canceled"),
                                0.0f, 2.0f, false});
    }
    prev_drop = state.playing_inputs.drop;
}

void sim_inventory_number_row(State& state) {
    bool nums[10] = {state.playing_inputs.num_row_1, state.playing_inputs.num_row_2,
                     state.playing_inputs.num_row_3, state.playing_inputs.num_row_4,
                     state.playing_inputs.num_row_5, state.playing_inputs.num_row_6,
                     state.playing_inputs.num_row_7, state.playing_inputs.num_row_8,
                     state.playing_inputs.num_row_9, state.playing_inputs.num_row_0};
    static bool prev[10] = {false};
    for (int i = 0; i < 10; ++i) {
        if (nums[i] && !prev[i]) {
            std::size_t idx = (i == 9) ? 9 : (std::size_t)i;
            state.inventory.set_selected_index(idx);
            const InvEntry* ent = state.inventory.selected_entry();
            if (state.drop_mode) {
                if (!ent) {
                    state.alerts.push_back({std::string("Slot empty"), 0.0f, 1.5f, false});
                } else if (state.player_vid) {
                    const Entity* p = state.entities.get(*state.player_vid);
                    if (p) {
                        glm::vec2 place_pos = ensure_not_in_block(state, p->pos);
                        if (ent->kind == INV_GUN) {
                            int gspr = -1;
                            std::string nm = "gun";
                            if (g_lua_mgr) {
                                const GunInstance* gi = state.guns.get(ent->vid);
                                if (gi) {
                                    for (auto const& g : g_lua_mgr->guns())
                                        if (g.type == gi->def_type) {
                                            nm = g.name;
                                            if (g_sprite_ids) {
                                                if (!g.sprite.empty() && g.sprite.find(':') != std::string::npos)
                                                    gspr = g_sprite_ids->try_get(g.sprite);
                                                else gspr = -1;
                                            }
                                            break;
                                        }
                                }
                            }
                            if (state.player_vid) {
                                Entity* pme = state.entities.get_mut(*state.player_vid);
                                if (pme && pme->equipped_gun_vid.has_value() &&
                                    pme->equipped_gun_vid->id == ent->vid.id &&
                                    pme->equipped_gun_vid->version == ent->vid.version) {
                                    pme->equipped_gun_vid.reset();
                                }
                            }
                            if (g_lua_mgr && state.player_vid) {
                                if (const GunInstance* gi = state.guns.get(ent->vid)) {
                                    if (auto* plent = state.entities.get_mut(*state.player_vid))
                                        g_lua_mgr->call_gun_on_drop(gi->def_type, state, *plent);
                                }
                            }
                            state.ground_guns.spawn(ent->vid, place_pos, gspr);
                            if (state.player_vid) {
                                if (auto* pm = state.metrics_for(*state.player_vid))
                                    pm->guns_dropped += 1;
                            }
                            state.inventory.remove_slot(idx);
                            state.alerts.push_back({std::string("Dropped gun: ") + nm, 0.0f, 2.0f, false});
                        } else {
                            if (const ItemInstance* inst = state.items.get(ent->vid)) {
                                int def_type = inst->def_type;
                                std::string nm = "item";
                                if (g_lua_mgr) {
                                    for (auto const& d : g_lua_mgr->items())
                                        if (d.type == def_type) { nm = d.name; break; }
                                }
                                if (inst->count > 1) {
                                    if (auto* mut = state.items.get(ent->vid)) {
                                        mut->count -= 1;
                                    }
                                    if (auto nv = state.items.alloc()) {
                                        if (auto* newv = state.items.get(*nv)) {
                                            newv->active = true;
                                            newv->def_type = def_type;
                                            newv->count = 1;
                                            state.ground_items.spawn(*nv, place_pos);
                                        }
                                    }
                                } else {
                                    state.ground_items.spawn(ent->vid, place_pos);
                                    state.inventory.remove_slot(idx);
                                }
                                if (state.player_vid) {
                                    if (auto* pm = state.metrics_for(*state.player_vid))
                                        pm->items_dropped += 1;
                                }
                                state.alerts.push_back({std::string("Dropped item: ") + nm, 0.0f, 2.0f, false});
                            }
                        }
                    }
                }
            } else {
                // Normal path: select/equip
                if (!ent)
                    ; // empty slot: nothing to do
                else if (ent->kind == INV_GUN) {
                    if (state.player_vid) {
                        if (auto* p = state.entities.get_mut(*state.player_vid)) {
                            p->equipped_gun_vid = ent->vid;
                            // Optional: alert name
                            if (g_lua_mgr) {
                                if (const GunInstance* gi = state.guns.get(ent->vid)) {
                                    for (auto const& g : g_lua_mgr->guns())
                                        if (g.type == gi->def_type) {
                                            state.alerts.push_back({std::string("Equipped ") + g.name, 0.0f, 1.2f, false});
                                            break;
                                        }
                                }
                            }
                        }
                    }
                } else if (ent->kind == INV_ITEM) {
                    // For now: just selects the item (future: use/equip behavior)
                }
            }
        }
        prev[i] = nums[i];
    }
}

void sim_ground_repulsion(State& state) {
    for (auto& giA : state.ground_items.data())
        if (giA.active) {
            for (auto& giB : state.ground_items.data())
                if (&giA != &giB && giB.active) {
                    glm::vec2 ah = giA.size * 0.5f, bh = giB.size * 0.5f;
                    bool overlap = !((giA.pos.x + ah.x) <= (giB.pos.x - bh.x) ||
                                     (giA.pos.x - ah.x) >= (giB.pos.x + bh.x) ||
                                     (giA.pos.y + ah.y) <= (giB.pos.y - bh.y) ||
                                     (giA.pos.y - ah.y) >= (giB.pos.y + bh.y));
                    if (overlap) {
                        glm::vec2 d = giA.pos - giB.pos;
                        if (d.x == 0 && d.y == 0) d = {0.01f, 0.0f};
                        float len = std::sqrt(d.x * d.x + d.y * d.y);
                        if (len < 1e-3f) len = 1.0f;
                        d /= len;
                        giA.pos += d * 0.01f;
                        giB.pos -= d * 0.01f;
                    }
                }
        }
    for (auto& ga : state.ground_guns.data())
        if (ga.active) {
            for (auto& gb : state.ground_guns.data())
                if (&ga != &gb && gb.active) {
                    glm::vec2 ah = ga.size * 0.5f, bh = gb.size * 0.5f;
                    bool overlap = !((ga.pos.x + ah.x) <= (gb.pos.x - bh.x) ||
                                     (ga.pos.x - ah.x) >= (gb.pos.x + bh.x) ||
                                     (ga.pos.y + ah.y) <= (gb.pos.y - bh.y) ||
                                     (ga.pos.y - ah.y) >= (gb.pos.y + bh.y));
                    if (overlap) {
                        glm::vec2 d = ga.pos - gb.pos;
                        if (d.x == 0 && d.y == 0) d = {0.01f, 0.0f};
                        float len = std::sqrt(d.x * d.x + d.y * d.y);
                        if (len < 1e-3f) len = 1.0f;
                        d /= len;
                        ga.pos += d * 0.01f;
                        gb.pos -= d * 0.01f;
                    }
                }
        }

    // Cross separation between ground items and ground guns
    for (auto& gi : state.ground_items.data())
        if (gi.active) {
            glm::vec2 ih = gi.size * 0.5f;
            for (auto& gg : state.ground_guns.data())
                if (gg.active) {
                    glm::vec2 gh = gg.size * 0.5f;
                    bool overlap = !((gi.pos.x + ih.x) <= (gg.pos.x - gh.x) ||
                                     (gi.pos.x - ih.x) >= (gg.pos.x + gh.x) ||
                                     (gi.pos.y + ih.y) <= (gg.pos.y - gh.y) ||
                                     (gi.pos.y - ih.y) >= (gg.pos.y + gh.y));
                    if (overlap) {
                        glm::vec2 d = gi.pos - gg.pos;
                        if (d.x == 0 && d.y == 0) d = {0.01f, 0.0f};
                        float len = std::sqrt(d.x * d.x + d.y * d.y);
                        if (len < 1e-3f) len = 1.0f;
                        d /= len;
                        gi.pos += d * 0.01f;
                        gg.pos -= d * 0.01f;
                    }
                }
        }

    // Push ground items and guns away from crates
    for (auto& c : state.crates.data())
        if (c.active) {
            glm::vec2 ch = c.size * 0.5f;
            for (auto& gi : state.ground_items.data())
                if (gi.active) {
                    glm::vec2 ih = gi.size * 0.5f;
                    bool overlap = !((gi.pos.x + ih.x) <= (c.pos.x - ch.x) ||
                                     (gi.pos.x - ih.x) >= (c.pos.x + ch.x) ||
                                     (gi.pos.y + ih.y) <= (c.pos.y - ch.y) ||
                                     (gi.pos.y - ih.y) >= (c.pos.y + ch.y));
                    if (overlap) {
                        glm::vec2 d = gi.pos - c.pos;
                        if (d.x == 0 && d.y == 0) d = {0.01f, 0.0f};
                        float len = std::sqrt(d.x * d.x + d.y * d.y);
                        if (len < 1e-3f) len = 1.0f;
                        d /= len;
                        gi.pos += d * 0.012f;
                    }
                }
            for (auto& gg : state.ground_guns.data())
                if (gg.active) {
                    glm::vec2 gh = gg.size * 0.5f;
                    bool overlap = !((gg.pos.x + gh.x) <= (c.pos.x - ch.x) ||
                                     (gg.pos.x - gh.x) >= (c.pos.x + ch.x) ||
                                     (gg.pos.y + gh.y) <= (c.pos.y - ch.y) ||
                                     (gg.pos.y - gh.y) >= (c.pos.y + ch.y));
                    if (overlap) {
                        glm::vec2 d = gg.pos - c.pos;
                        if (d.x == 0 && d.y == 0) d = {0.01f, 0.0f};
                        float len = std::sqrt(d.x * d.x + d.y * d.y);
                        if (len < 1e-3f) len = 1.0f;
                        d /= len;
                        gg.pos += d * 0.012f;
                    }
                }
        }
}

void sim_update_crates_open(State& state) {
    if (!state.player_vid)
        return;
    const Entity* p = state.entities.get(*state.player_vid);
    if (!p)
        return;
    glm::vec2 ph = p->half_size();
    float pl = p->pos.x - ph.x, pr = p->pos.x + ph.x, pt = p->pos.y - ph.y, pb = p->pos.y + ph.y;
    for (auto& c : state.crates.data())
        if (c.active && !c.opened) {
            glm::vec2 ch = c.size * 0.5f;
            float cl = c.pos.x - ch.x, cr = c.pos.x + ch.x, ct = c.pos.y - ch.y, cb = c.pos.y + ch.y;
            bool overlap = !(pr <= cl || pl >= cr || pb <= ct || pt >= cb);
            float open_time = 5.0f;
            if (g_lua_mgr) {
                if (auto const* cd = g_lua_mgr->find_crate(c.def_type))
                    open_time = cd->open_time;
            }
            if (overlap) {
                c.open_progress = std::min(open_time, c.open_progress + TIMESTEP);
            } else {
                c.open_progress = std::max(0.0f, c.open_progress - TIMESTEP * 0.5f);
            }
            if (c.open_progress >= open_time) {
                c.opened = true;
                c.active = false;
                state.metrics.crates_opened += 1;
                // drop loot at crate position using drop tables
                glm::vec2 pos = c.pos;
                if (g_lua_mgr) {
                    DropTables dt{};
                    bool have = false;
                    if (auto const* cd = g_lua_mgr->find_crate(c.def_type)) {
                        dt = cd->drops;
                        have = true;
                    }
                    if (!have)
                        dt = g_lua_mgr->drops();
                    static thread_local std::mt19937 rng{std::random_device{}()};
                    std::uniform_real_distribution<float> U(0.0f, 1.0f);
                    auto pick_weighted = [&](const std::vector<DropEntry>& v) -> int {
                        if (v.empty())
                            return -1;
                        float sum = 0.0f;
                        for (auto const& de : v) sum += de.weight;
                        if (sum <= 0.0f)
                            return -1;
                        std::uniform_real_distribution<float> du(0.0f, sum);
                        float r = du(rng), acc = 0.0f;
                        for (auto const& de : v) { acc += de.weight; if (r <= acc) return de.type; }
                        return v.back().type;
                    };
                    float cval = U(rng);
                    if (cval < 0.6f && !dt.items.empty()) {
                        int t = pick_weighted(dt.items);
                        if (t >= 0) {
                            auto it = std::find_if(g_lua_mgr->items().begin(), g_lua_mgr->items().end(),
                                                   [&](const ItemDef& d) { return d.type == t; });
                            if (it != g_lua_mgr->items().end()) {
                                if (auto iv = state.items.spawn_from_def(*it, 1)) {
                                    state.ground_items.spawn(*iv, pos);
                                    state.metrics.items_spawned += 1;
                                }
                            }
                        }
                    } else if (!dt.guns.empty()) {
                        int t = pick_weighted(dt.guns);
                        if (t >= 0) {
                            auto ig = std::find_if(g_lua_mgr->guns().begin(), g_lua_mgr->guns().end(),
                                                   [&](const GunDef& g) { return g.type == t; });
                            if (ig != g_lua_mgr->guns().end()) {
                                if (auto gv = state.guns.spawn_from_def(*ig)) {
                                    int sid = -1;
                                    if (g_sprite_ids) {
                                        if (!ig->sprite.empty() && ig->sprite.find(':') != std::string::npos)
                                            sid = g_sprite_ids->try_get(ig->sprite);
                                    }
                                    state.ground_guns.spawn(*gv, pos, sid);
                                    state.metrics.guns_spawned += 1;
                                }
                            }
                        }
                    }
                    if (state.player_vid) {
                        if (auto* plent = state.entities.get_mut(*state.player_vid))
                            g_lua_mgr->call_crate_on_open(c.def_type, state, *plent);
                    }
                }
            }
        }
}

void sim_step_projectiles(State& state, Projectiles& projectiles) {
    struct HitInfo {
        std::size_t eid;
        std::optional<VID> owner;
        float base_damage;
        float armor_pen;      // 0..1
        float shield_mult;
        int ammo_type;
        float travel_dist;
        int proj_def_type;
    };
    std::vector<HitInfo> hits;
    projectiles.step(
        TIMESTEP, state.stage, state.entities.data(),
        [&](Projectile& pr, const Entity& hit) -> bool {
            if (g_lua_mgr && pr.def_type)
                g_lua_mgr->call_projectile_on_hit_entity(pr.def_type);
            if (g_lua_mgr && pr.ammo_type)
                g_lua_mgr->call_ammo_on_hit_entity(pr.ammo_type), g_lua_mgr->call_ammo_on_hit(pr.ammo_type);
            if (pr.owner) {
                if (auto* pm = state.metrics_for(*pr.owner))
                    pm->shots_hit += 1;
            }
            hits.push_back(HitInfo{hit.vid.id, pr.owner, pr.base_damage, pr.armor_pen,
                                    pr.shield_mult, pr.ammo_type, pr.distance_travelled,
                                    pr.def_type});
            bool stop = true;
            if (pr.pierce_remaining > 0) { pr.pierce_remaining -= 1; stop = false; }
            return stop;
        },
        [&](Projectile& pr) {
            if (g_lua_mgr && pr.def_type)
                g_lua_mgr->call_projectile_on_hit_tile(pr.def_type);
            if (g_lua_mgr && pr.ammo_type)
                g_lua_mgr->call_ammo_on_hit_tile(pr.ammo_type), g_lua_mgr->call_ammo_on_hit(pr.ammo_type);
        });

    for (auto h : hits) {
        auto id = h.eid;
        if (id >= state.entities.data().size())
            continue;
        auto& e = state.entities.data()[id];
        if (!e.active)
            continue;
        if (e.type_ == ids::ET_NPC || e.type_ == ids::ET_PLAYER) {
            if (e.health == 0) e.health = 3;
            if (e.max_hp == 0) e.max_hp = 3;
            float dmg = h.base_damage;
            float ap = std::clamp(h.armor_pen * 100.0f, 0.0f, 100.0f);
            float shield_mult = h.shield_mult;
            if (g_lua_mgr && h.ammo_type != 0) {
                if (auto const* ad = g_lua_mgr->find_ammo(h.ammo_type)) {
                    if (ad->falloff_end > ad->falloff_start && ad->falloff_end > 0.0f) {
                        float m = 1.0f;
                        if (h.travel_dist <= ad->falloff_start) m = 1.0f;
                        else if (h.travel_dist >= ad->falloff_end) m = ad->falloff_min_mult;
                        else {
                            float t = (h.travel_dist - ad->falloff_start) / (ad->falloff_end - ad->falloff_start);
                            m = 1.0f + t * (ad->falloff_min_mult - 1.0f);
                        }
                        if (m < 0.0f) m = 0.0f;
                        dmg *= m;
                    }
                }
            }
            if (dmg <= 0.0f) dmg = 1.0f;
            if (e.type_ == ids::ET_PLAYER) {
                if (e.stats.shield_max > 0.0f && e.shield > 0.0f) {
                    float took = std::min(e.shield, (float)(dmg * shield_mult));
                    e.shield -= took;
                    if (auto* pm = state.metrics_for(e.vid)) pm->damage_taken_shield += (std::uint64_t)std::lround(took);
                    if (h.owner) if (auto* om = state.metrics_for(*h.owner)) om->damage_dealt += (std::uint64_t)std::lround(took);
                    dmg -= took; if (dmg < 0.0f) dmg = 0.0f;
                }
                if (dmg > 0.0f && e.stats.plates > 0) { e.stats.plates -= 1; if (auto* pm = state.metrics_for(e.vid)) pm->plates_consumed += 1; dmg = 0.0f; }
                if (dmg > 0.0f) {
                    float reduction = std::max(0.0f, e.stats.armor - (float)ap);
                    reduction = std::min(75.0f, reduction);
                    float scale = 1.0f - reduction * 0.01f;
                    int delt = (int)std::ceil((double)dmg * (double)scale);
                    std::uint32_t before = e.health;
                    e.health = (e.health > (uint32_t)delt) ? (e.health - (uint32_t)delt) : 0u;
                    if (auto* pm = state.metrics_for(e.vid)) pm->damage_taken_hp += (std::uint64_t)(before - e.health);
                    if (h.owner) if (auto* om = state.metrics_for(*h.owner)) om->damage_dealt += (std::uint64_t)delt;
                }
            } else {
                if (e.stats.plates > 0) { e.stats.plates -= 1; dmg = 0.0f; }
                if (dmg > 0.0f) {
                    float reduction = std::max(0.0f, e.stats.armor - (float)ap);
                    reduction = std::min(75.0f, reduction);
                    float scale = 1.0f - reduction * 0.01f;
                    int delt = (int)std::ceil((double)dmg * (double)scale);
                    e.health = (e.health > (uint32_t)delt) ? (e.health - (uint32_t)delt) : 0u;
                    if (h.owner) if (auto* pm = state.metrics_for(*h.owner)) pm->damage_dealt += (std::uint64_t)delt;
                }
            }
        }
        e.time_since_damage = 0.0f;
        if (e.type_ == ids::ET_NPC && e.health == 0) {
            glm::vec2 pos = e.pos;
            e.active = false;
            state.metrics.enemies_slain += 1;
            state.metrics.enemies_slain_by_type[(int)e.type_] += 1;
            if (h.owner) if (auto* pm = state.metrics_for(*h.owner)) pm->enemies_slain += 1;
            static thread_local std::mt19937 rng{std::random_device{}()};
            std::uniform_real_distribution<float> U(0.0f, 1.0f);
            if (U(rng) < 0.5f && g_lua_mgr) {
                glm::vec2 place_pos = ensure_not_in_block(state, pos);
                const auto& dt = g_lua_mgr->drops();
                auto pick_weighted = [&](const std::vector<DropEntry>& v) -> int {
                    if (v.empty())
                        return -1;
                    float sum = 0.0f;
                    for (auto const& de : v)
                        sum += de.weight;
                    if (sum <= 0.0f)
                        return -1;
                    std::uniform_real_distribution<float> du(0.0f, sum);
                    float r = du(rng), acc = 0.0f;
                    for (auto const& de : v) {
                        acc += de.weight;
                        if (r <= acc)
                            return de.type;
                    }
                    return v.back().type;
                };
                if (!dt.powerups.empty() || !dt.items.empty() || !dt.guns.empty()) {
                    float c = U(rng);
                    if (c < 0.5f && !dt.powerups.empty()) {
                        int t = pick_weighted(dt.powerups);
                        if (t >= 0) {
                            auto it = std::find_if(g_lua_mgr->powerups().begin(), g_lua_mgr->powerups().end(), [&](const PowerupDef& p) { return p.type == t; });
                            if (it != g_lua_mgr->powerups().end()) {
                                auto* p = state.pickups.spawn((std::uint32_t)it->type, it->name, place_pos);
                                if (p) {
                                    state.metrics.powerups_spawned += 1;
                                    if (g_sprite_ids) {
                                        if (!it->sprite.empty() && it->sprite.find(':') != std::string::npos)
                                            p->sprite_id = g_sprite_ids->try_get(it->sprite);
                                        else p->sprite_id = -1;
                                    }
                                }
                            }
                        }
                    } else if (c < 0.85f && !dt.items.empty()) {
                        int t = pick_weighted(dt.items);
                        if (t >= 0) {
                            auto it = std::find_if(g_lua_mgr->items().begin(), g_lua_mgr->items().end(), [&](const ItemDef& d) { return d.type == t; });
                            if (it != g_lua_mgr->items().end()) {
                                if (auto iv = state.items.spawn_from_def(*it, 1)) {
                                    state.ground_items.spawn(*iv, place_pos);
                                    state.metrics.items_spawned += 1;
                                }
                            }
                        }
                    } else if (!dt.guns.empty()) {
                        int t = pick_weighted(dt.guns);
                        if (t >= 0) {
                            auto itg = std::find_if(g_lua_mgr->guns().begin(), g_lua_mgr->guns().end(), [&](const GunDef& g) { return g.type == t; });
                            if (itg != g_lua_mgr->guns().end()) {
                                if (auto inst = state.guns.spawn_from_def(*itg)) {
                                    int gspr = -1;
                                    if (g_sprite_ids) {
                                        if (!itg->sprite.empty() && itg->sprite.find(':') != std::string::npos)
                                            gspr = g_sprite_ids->try_get(itg->sprite);
                                        else gspr = -1;
                                    }
                                    state.ground_guns.spawn(*inst, place_pos, gspr);
                                    state.metrics.guns_spawned += 1;
                                }
                            }
                        }
                    }
                } else if (U(rng) < 0.5f && !g_lua_mgr->powerups().empty()) {
                    std::uniform_int_distribution<int> di(0, (int)g_lua_mgr->powerups().size() - 1);
                    auto& pu = g_lua_mgr->powerups()[(size_t)di(rng)];
                    auto* p = state.pickups.spawn((std::uint32_t)pu.type, pu.name, place_pos);
                    if (p) {
                        state.metrics.powerups_spawned += 1;
                        if (g_sprite_ids) {
                            if (!pu.sprite.empty() && pu.sprite.find(':') != std::string::npos)
                                p->sprite_id = g_sprite_ids->try_get(pu.sprite);
                            else p->sprite_id = -1;
                        }
                    }
                }
            }
        }
    }
}

void sim_handle_pickups(State& state, SoundStore& sounds) {
    if (state.mode != ids::MODE_PLAYING || !state.player_vid)
        return;
    const Entity* p = state.entities.get(*state.player_vid);
    if (!p)
        return;
    glm::vec2 ph = p->half_size();
    float pl = p->pos.x - ph.x, pr = p->pos.x + ph.x;
    float pt = p->pos.y - ph.y, pb = p->pos.y + ph.y;
    static bool prev_pick = false;
    bool now_pick = state.playing_inputs.pick_up;
    if (now_pick && !prev_pick && state.pickup_lockout == 0.0f) {
        bool did_pick = false;
        enum class PickKind { None, Gun, Item };
        PickKind best_kind = PickKind::None;
        std::size_t best_index = (std::size_t)-1;
        float best_area = 0.0f;
        auto overlap_area = [&](float al, float at, float ar, float ab, float bl, float bt, float br, float bb) -> float {
            float xl = std::max(al, bl);
            float xr = std::min(ar, br);
            float yt = std::max(at, bt);
            float yb = std::min(ab, bb);
            float w = xr - xl;
            float h = yb - yt;
            if (w <= 0.0f || h <= 0.0f)
                return 0.0f;
            return w * h;
        };
        for (std::size_t i = 0; i < state.ground_guns.data().size(); ++i) {
            auto const& gg = state.ground_guns.data()[i];
            if (!gg.active) continue;
            glm::vec2 gh = gg.size * 0.5f;
            float gl = gg.pos.x - gh.x, gr = gg.pos.x + gh.x;
            float gt = gg.pos.y - gh.y, gb = gg.pos.y + gh.y;
            float area = overlap_area(pl, pt, pr, pb, gl, gt, gr, gb);
            if (area > best_area) { best_area = area; best_kind = PickKind::Gun; best_index = i; }
        }
        for (std::size_t i = 0; i < state.ground_items.data().size(); ++i) {
            auto const& gi = state.ground_items.data()[i];
            if (!gi.active) continue;
            glm::vec2 gh = gi.size * 0.5f;
            float gl = gi.pos.x - gh.x, gr = gi.pos.x + gh.x;
            float gt = gi.pos.y - gh.y, gb = gi.pos.y + gh.y;
            float area = overlap_area(pl, pt, pr, pb, gl, gt, gr, gb);
            if (area > best_area) { best_area = area; best_kind = PickKind::Item; best_index = i; }
        }
        if (best_kind == PickKind::Gun) {
            auto& gg = state.ground_guns.data()[best_index];
            bool ok = state.inventory.insert_existing(INV_GUN, gg.gun_vid);
            std::string nm = "gun";
            if (g_lua_mgr) {
                if (const GunInstance* gi = state.guns.get(gg.gun_vid)) {
                    for (auto const& g : g_lua_mgr->guns()) if (g.type == gi->def_type) { nm = g.name; break; }
                }
            }
            if (ok) {
                gg.active = false;
                did_pick = true;
                state.alerts.push_back({std::string("Picked up ") + nm, 0.0f, 2.0f, false});
                if (state.player_vid) if (auto* pm = state.metrics_for(*state.player_vid)) pm->guns_picked += 1;
                if (const GunInstance* ggi = state.guns.get(gg.gun_vid)) {
                    const GunDef* gd = nullptr;
                    if (g_lua_mgr) for (auto const& g : g_lua_mgr->guns()) if (g.type == ggi->def_type) { gd = &g; break; }
                    if (g_lua_mgr && state.player_vid) if (auto* plent = state.entities.get_mut(*state.player_vid)) g_lua_mgr->call_gun_on_pickup(ggi->def_type, state, *plent);
                    if (gd) sounds.play(gd->sound_pickup.empty() ? "base:drop" : gd->sound_pickup); else sounds.play("base:drop");
                }
            } else {
                state.alerts.push_back({"Inventory full", 0.0f, 1.5f, false});
            }
        } else if (best_kind == PickKind::Item) {
            auto& gi = state.ground_items.data()[best_index];
            std::string nm = "item";
            int maxc = 1;
            const ItemInstance* pick = state.items.get(gi.item_vid);
            if (g_lua_mgr && pick) {
                for (auto const& d : g_lua_mgr->items()) if (d.type == pick->def_type) { nm = d.name; maxc = d.max_count; break; }
            }
            bool fully_merged = false;
            if (pick) {
                for (auto& e : state.inventory.entries) {
                    if (e.kind != INV_ITEM) continue;
                    ItemInstance* tgt = state.items.get(e.vid);
                    if (!tgt) continue;
                    if (tgt->def_type != pick->def_type) continue;
                    if (tgt->modifiers_hash != pick->modifiers_hash) continue;
                    if (tgt->use_cooldown_countdown > 0.0f || pick->use_cooldown_countdown > 0.0f) continue;
                    if (tgt->count >= (uint32_t)maxc) continue;
                    uint32_t space = (uint32_t)maxc - tgt->count;
                    uint32_t xfer = std::min(space, pick->count);
                    tgt->count += xfer;
                    if (auto* pmut = state.items.get(gi.item_vid)) pmut->count -= xfer;
                    if (auto* after = state.items.get(gi.item_vid)) {
                        if (after->count == 0) { state.items.free(gi.item_vid); gi.active = false; fully_merged = true; }
                    }
                    if (xfer > 0) break;
                }
            }
            if (!fully_merged) {
                bool ok = state.inventory.insert_existing(INV_ITEM, gi.item_vid);
                if (ok) {
                    gi.active = false;
                    did_pick = true;
                    state.alerts.push_back({std::string("Picked up ") + nm, 0.0f, 2.0f, false});
                    if (g_lua_mgr && pick && state.player_vid) if (auto* plent = state.entities.get_mut(*state.player_vid)) g_lua_mgr->call_item_on_pickup(pick->def_type, state, *plent);
                    if (g_lua_mgr && pick) {
                        const ItemDef* idf = nullptr;
                        for (auto const& d : g_lua_mgr->items()) if (d.type == pick->def_type) { idf = &d; break; }
                        if (idf) sounds.play(idf->sound_pickup.empty() ? "base:drop" : idf->sound_pickup); else sounds.play("base:drop");
                    }
                    if (state.player_vid) if (auto* pm = state.metrics_for(*state.player_vid)) pm->items_picked += 1;
                } else {
                    state.alerts.push_back({std::string("Inventory full"), 0.0f, 1.5f, false});
                }
            }
        }
        if (did_pick) state.pickup_lockout = PICKUP_DEBOUNCE_SECONDS;
    }
    prev_pick = now_pick;
}
