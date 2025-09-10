#pragma once

#include "luamgr.hpp"
#include "pool.hpp"

#include <cstdint>
#include <glm/glm.hpp>
#include <vector>
#include <random>

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
  // Accuracy: accumulated recoil spread (degrees)
  float spread_recoil_deg{0.0f};
  // Active reload state
  bool reloading{false};
  float reload_progress{0.0f}; // 0..1 after eject
  float reload_eject_remaining{0.0f};
  float reload_total_time{0.0f};
  float ar_window_start{0.0f}; // fraction 0..1
  float ar_window_end{0.0f};   // fraction 0..1
  bool ar_consumed{false};
  bool ar_failed_attempt{false};
  // Optional ticking accumulator (opt-in via def.tick_rate_hz)
  float tick_acc{0.0f};
  // Selected ammo type (Lua AmmoDef::type). 0 means unset/default.
  int ammo_type{0};
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
            // Pick ammo on gun generation if compat list provided
            gi->ammo_type = 0;
            if (!d.compatible_ammo.empty()) {
                static thread_local std::mt19937 rng{std::random_device{}()};
                float sum = 0.0f;
                for (auto const& ac : d.compatible_ammo) sum += ac.weight;
                if (sum > 0.0f) {
                    std::uniform_real_distribution<float> du(0.0f, sum);
                    float r = du(rng), acc = 0.0f;
                    for (auto const& ac : d.compatible_ammo) {
                        acc += ac.weight;
                        if (r <= acc) { gi->ammo_type = ac.type; break; }
                    }
                    if (gi->ammo_type == 0)
                        gi->ammo_type = d.compatible_ammo.back().type;
                }
            }
        }
        return v;
    }
};

struct GroundGun {
    bool active{false};
    VID gun_vid{};
    glm::vec2 pos{0.0f, 0.0f};
    glm::vec2 size{0.125f, 0.125f};
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
