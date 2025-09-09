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
};

class Projectiles {
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
                return &pr;
            }
        }
        return nullptr;
    }

    void clear() {
        for (auto& pr : items)
            pr.active = false;
    }

    template <class HitEntityFn, class HitTileFn>
    void step(float dt, const Stage& stage, const std::vector<Entity>& ents, HitEntityFn on_hit,
              HitTileFn on_hit_tile) {
        for (auto& pr : items) {
            if (!pr.active)
                continue;
            int steps = std::max(1, pr.physics_steps);
            glm::vec2 step_dpos = pr.vel * (dt / static_cast<float>(steps));
            for (int s = 0; s < steps; ++s) {
                // Separate-axis move for projectiles
                glm::vec2 next = pr.pos;
                // X axis
                next.x += step_dpos.x;
                {
                    glm::vec2 half = 0.5f * pr.size;
                    glm::vec2 tl = {next.x - half.x, pr.pos.y - half.y};
                    glm::vec2 br = {next.x + half.x, pr.pos.y + half.y};
                    int minx = (int)floorf(tl.x), miny = (int)floorf(tl.y),
                        maxx = (int)floorf(br.x), maxy = (int)floorf(br.y);
                    bool blocked = false;
                    for (int y = miny; y <= maxy && !blocked; ++y)
                        for (int x = minx; x <= maxx; ++x)
                            if (stage.in_bounds(x, y) && stage.at(x, y).blocks_projectiles()) {
                                blocked = true;
                            }
                    if (blocked) {
                        on_hit_tile(pr);
                        pr.active = false;
                        break;
                    } else {
                        pr.pos.x = next.x;
                    }
                }
                if (!pr.active)
                    break;
                // Y axis
                next.y += step_dpos.y;
                {
                    glm::vec2 half = 0.5f * pr.size;
                    glm::vec2 tl = {pr.pos.x - half.x, next.y - half.y};
                    glm::vec2 br = {pr.pos.x + half.x, next.y + half.y};
                    int minx = (int)floorf(tl.x), miny = (int)floorf(tl.y),
                        maxx = (int)floorf(br.x), maxy = (int)floorf(br.y);
                    bool blocked = false;
                    for (int y = miny; y <= maxy && !blocked; ++y)
                        for (int x = minx; x <= maxx; ++x)
                            if (stage.in_bounds(x, y) && stage.at(x, y).blocks_projectiles()) {
                                blocked = true;
                            }
                    if (blocked) {
                        on_hit_tile(pr);
                        pr.active = false;
                        break;
                    } else {
                        pr.pos.y = next.y;
                    }
                }
                if (!pr.active)
                    break;
                // entity hit
                for (auto const& e : ents) {
                    if (!e.active)
                        continue;
                    if (pr.owner && pr.owner->id == e.vid.id && pr.owner->version == e.vid.version)
                        continue;
                    glm::vec2 eh = e.half_size();
                    glm::vec2 etl = e.pos - eh;
                    glm::vec2 ebr = e.pos + eh;
                    glm::vec2 half = 0.5f * pr.size;
                    glm::vec2 tl = pr.pos - half;
                    glm::vec2 br = pr.pos + half;
                    bool overlap = !(br.x < etl.x || tl.x > ebr.x || br.y < etl.y || tl.y > ebr.y);
                    if (overlap) {
                        on_hit(pr, e);
                        pr.active = false;
                        break;
                    }
                }
                if (!pr.active)
                    break;
            }
        }
    }

    std::vector<Projectile> items;
};
