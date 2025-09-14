#pragma once

#include "entity.hpp"
#include "stage.hpp"
#include "types.hpp"

#include <glm/glm.hpp>
#include <optional>
#include <vector>

struct Projectile {
    bool active{false};
    glm::vec2 pos{0.0f, 0.0f};
    glm::vec2 vel{0.0f, 0.0f};
    glm::vec2 size{0.25f, 0.25f};
    float rot{0.0f};
    int sprite_id{-1};
    int physics_steps{1};
    std::optional<VID> owner{};
    int def_type{0};
    // Ammo context
    int ammo_type{0};
    float base_damage{1.0f};
    float armor_pen{0.0f}; // 0..1
    float shield_mult{1.0f};
    float distance_travelled{0.0f};
    float max_range_units{0.0f}; // 0 => unlimited
    int pierce_remaining{0}; // entities this projectile can still pass through
};

struct Projectiles {
  public:
    static constexpr std::size_t MAX = 1024;
    Projectiles() : items(MAX) {
    }

    Projectile* spawn(glm::vec2 p, glm::vec2 v, glm::vec2 sz, int steps = 1, int def_type = 0) {
        for (auto& pr : items) {
            if (!pr.active) {
                pr.active = true;
                pr.pos = p;
                pr.vel = v;
                pr.size = sz;
                pr.physics_steps = steps;
                pr.def_type = def_type;
                pr.distance_travelled = 0.0f;
                pr.max_range_units = 0.0f;
                pr.ammo_type = 0;
                pr.base_damage = 1.0f;
                pr.armor_pen = 0.0f;
                pr.shield_mult = 1.0f;
                pr.pierce_remaining = 0;
                return &pr;
            }
        }
        return nullptr;
    }

    void clear() {
        for (auto& pr : items)
            pr.active = false;
    }

    template <typename HitEntityFn, typename HitTileFn>
    void step(float dt,
              const Stage& stage,
              const std::vector<Entity>& ents,
              HitEntityFn&& on_hit,        // bool(Projectile&, const Entity&)
              HitTileFn&& on_hit_tile) {   // void(Projectile&)
        for (auto& pr : items) {
            if (!pr.active) continue;
            int steps = std::max(1, pr.physics_steps);
            glm::vec2 step_dpos = pr.vel * (dt / static_cast<float>(steps));
            for (int s = 0; s < steps; ++s) {
                pr.distance_travelled += std::sqrt(step_dpos.x*step_dpos.x + step_dpos.y*step_dpos.y);
                if (pr.max_range_units > 0.0f && pr.distance_travelled >= pr.max_range_units) {
                    pr.active = false; break;
                }
                // X axis
                glm::vec2 next = pr.pos;
                next.x += step_dpos.x;
                {
                    glm::vec2 half = 0.5f * pr.size;
                    glm::vec2 tl = {next.x - half.x, pr.pos.y - half.y};
                    glm::vec2 br = {next.x + half.x, pr.pos.y + half.y};
                    int minx = (int)std::floor(tl.x), miny = (int)std::floor(tl.y);
                    int maxx = (int)std::floor(br.x), maxy = (int)std::floor(br.y);
                    bool blocked = false;
                    for (int y = miny; y <= maxy && !blocked; ++y)
                        for (int x = minx; x <= maxx; ++x)
                            if (stage.in_bounds(x, y) && stage.at(x, y).blocks_projectiles())
                                blocked = true;
                    if (blocked) { on_hit_tile(pr); pr.active = false; break; }
                    pr.pos.x = next.x;
                }
                if (!pr.active) break;
                // Y axis
                next.y += step_dpos.y;
                {
                    glm::vec2 half = 0.5f * pr.size;
                    glm::vec2 tl = {pr.pos.x - half.x, next.y - half.y};
                    glm::vec2 br = {pr.pos.x + half.x, next.y + half.y};
                    int minx = (int)std::floor(tl.x), miny = (int)std::floor(tl.y);
                    int maxx = (int)std::floor(br.x), maxy = (int)std::floor(br.y);
                    bool blocked = false;
                    for (int y = miny; y <= maxy && !blocked; ++y)
                        for (int x = minx; x <= maxx; ++x)
                            if (stage.in_bounds(x, y) && stage.at(x, y).blocks_projectiles())
                                blocked = true;
                    if (blocked) { on_hit_tile(pr); pr.active = false; break; }
                    pr.pos.y = next.y;
                }
                if (!pr.active) break;
                // Entities
                for (auto const& e : ents) {
                    if (!e.active) continue;
                    if (pr.owner && pr.owner->id == e.vid.id && pr.owner->version == e.vid.version)
                        continue;
                    glm::vec2 eh = e.half_size();
                    glm::vec2 etl = e.pos - eh, ebr = e.pos + eh;
                    glm::vec2 half = 0.5f * pr.size;
                    glm::vec2 tl = pr.pos - half, br = pr.pos + half;
                    bool overlap = !(br.x < etl.x || tl.x > ebr.x || br.y < etl.y || tl.y > ebr.y);
                    if (overlap) {
                        bool stop = on_hit(pr, e);
                        if (stop) { pr.active = false; break; }
                    }
                }
                if (!pr.active) break;
            }
        }
    }

    std::vector<Projectile> items;
};
