#pragma once

#include "luamgr.hpp"
#include "pool.hpp"

#include <cstdint>
#include <glm/glm.hpp>
#include <vector>

struct GunInstance {
    bool active{false};
    int def_type{0}; // matches GunDef::type from Lua
    int current_mag{0};
    int ammo_reserve{0};
    float heat{0.0f};
    bool jammed{false};
    float unjam_progress{0.0f}; // 0..1 when mashing space
    int burst_remaining{0};
    float burst_timer{0.0f};
    float reload_timer{0.0f};
};

class GunsPool : public Pool<GunInstance, 1024> {
  public:
    std::optional<VID> spawn_from_def(const GunDef& d) {
        auto v = alloc();
        if (!v)
            return std::nullopt;
        if (auto* gi = get(*v)) {
            gi->def_type = d.type;
            gi->current_mag = d.mag;
            gi->ammo_reserve = d.ammo_max;
            gi->heat = 0.0f;
        }
        return v;
    }
};

struct GroundGun {
    bool active{false};
    VID gun_vid{};
    glm::vec2 pos{0.0f, 0.0f};
    glm::vec2 size{0.25f, 0.25f};
    int sprite_id{-1};
};

class GroundGunsPool {
  public:
    static constexpr std::size_t MAX = 1024;
    GroundGunsPool() {
        items.resize(MAX);
    }
    GroundGun* spawn(VID gun_vid, glm::vec2 p, int sprite_id) {
        for (auto& g : items)
            if (!g.active) {
                g.active = true;
                g.gun_vid = gun_vid;
                g.pos = p;
                g.sprite_id = sprite_id;
                return &g;
            }
        return nullptr;
    }
    void clear() {
        for (auto& g : items)
            g.active = false;
    }
    std::vector<GroundGun>& data() {
        return items;
    }
    const std::vector<GroundGun>& data() const {
        return items;
    }

  private:
    std::vector<GroundGun> items;
};
