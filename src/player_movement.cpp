// Movement and collision implementation for players and NPCs.
#include "globals.hpp"
#include "state.hpp"
#include "settings.hpp"
#include "luamgr.hpp"

#include <algorithm>
#include <cmath>
#include <random>

void update_movement_and_collision() {
    for (auto& e : ss->entities.data()) {
        if (!e.active)
            continue;
        e.time_since_damage += TIMESTEP;
        if (e.type_ == ids::ET_PLAYER) {
            glm::vec2 dir{0.0f, 0.0f};
            if (ss->playing_inputs.left)  dir.x -= 1.0f;
            if (ss->playing_inputs.right) dir.x += 1.0f;
            if (ss->playing_inputs.up)    dir.y -= 1.0f;
            if (ss->playing_inputs.down)  dir.y += 1.0f;
            if (dir.x != 0.0f || dir.y != 0.0f)
                dir = glm::normalize(dir);
            float scale = (e.stats.move_speed > 0.0f) ? (e.stats.move_speed / 350.0f) : 1.0f;
            ss->dash_timer = std::max(0.0f, ss->dash_timer - TIMESTEP);
            if (ss->dash_stocks < ss->dash_max) {
                ss->dash_refill_timer += TIMESTEP;
                while (ss->dash_refill_timer >= DASH_COOLDOWN_SECONDS && ss->dash_stocks < ss->dash_max) {
                    ss->dash_refill_timer -= DASH_COOLDOWN_SECONDS;
                    ss->dash_stocks += 1;
                }
            } else {
                ss->dash_refill_timer = 0.0f;
            }
            static bool prev_dash = false;
            bool now_dash = ss->playing_inputs.dash;
            if (now_dash && !prev_dash && ss->dash_stocks > 0) {
                if (dir.x != 0.0f || dir.y != 0.0f) {
                    ss->dash_dir = dir;
                    ss->dash_timer = DASH_TIME_SECONDS;
                    ss->dash_stocks -= 1;
                    if (ss->player_vid) {
                        if (auto* pm = ss->metrics_for(*ss->player_vid)) {
                            pm->dashes_used += 1;
                            pm->dash_distance += DASH_SPEED_UNITS_PER_SEC * DASH_TIME_SECONDS;
                        }
                    }
                    ss->reticle_shake = std::max(ss->reticle_shake, 8.0f);
                    if (luam && ss->player_vid) {
                        if (auto* pl = ss->entities.get_mut(*ss->player_vid))
                            luam->call_on_dash(*pl);
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
            if (ss->dash_timer > 0.0f) {
                e.vel = ss->dash_dir * DASH_SPEED_UNITS_PER_SEC;
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
                for (int ty = miny; ty <= maxy && !blocked; ++ty)
                    for (int tx = minx; tx <= maxx; ++tx) {
                        if (!ss->stage.in_bounds(tx, ty)) { blocked = true; break; }
                        if (ss->stage.at(tx, ty).blocks_entities()) { blocked = true; break; }
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
                for (int ty = miny; ty <= maxy && !blocked; ++ty)
                    for (int tx = minx; tx <= maxx; ++tx) {
                        if (!ss->stage.in_bounds(tx, ty)) { blocked = true; break; }
                        if (ss->stage.at(tx, ty).blocks_entities()) { blocked = true; break; }
                    }
                if (!blocked) e.pos.y = next_y; else e.vel.y = 0.0f;
            }
        }
    }
}
