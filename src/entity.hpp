#pragma once

#include "types.hpp"

#include <glm/glm.hpp>
#include <optional>

struct Entity {
    bool active{false};
    bool marked_for_destruction{false};
    int type_{ids::ET_NONE};
    VID vid{};
    bool impassable{false};

    glm::vec2 pos{0.0f, 0.0f};
    glm::vec2 vel{0.0f, 0.0f};
    glm::vec2 size{1.0f, 1.0f};
    float rot{0.0f};
    bool horizontal_flip{false};

    uint32_t health{1};
    uint32_t max_hp{1};
    float shield{0.0f}; // current shield (optional)
    float time_since_damage{0.0f};
    int physics_steps{1};
    int sprite_id{-1};

    inline glm::vec2 half_size() const {
        return 0.5f * size;
    }

    // Basic stat block (inspired by SYNTHETIK style). Units are engine-defined.
    struct Stats {
        // Survivability
        float max_health{1000.0f};
        float health_regen{0.0f};
        float shield_max{700.0f};
        float shield_regen{250.0f};
        float armor{0.0f}; // percent reduction (0..75 caps benefits in original ref)

        // Mobility
        float move_speed{350.0f}; // units per second (reference scale)
        float dodge{3.0f};        // percent chance

        // Economy/modifiers
        float scavenging{100.0f};
        float currency{100.0f};  // 100 => 1x
        float ammo_gain{100.0f}; // 100 => 1x
        float luck{100.0f};

        // Combat
        float crit_chance{3.0f};   // percent
        float crit_damage{200.0f}; // 200 => 2x
        float headshot_damage{200.0f};
        float damage_absorb{100.0f}; // 100 => 1/1
        float damage_output{100.0f}; // 100 => 1x
        float healing{100.0f};       // 100 => 1x
        float accuracy{100.0f};      // divider basis
        float terror_level{100.0f};

        // Armor plates: each consumes one hit before applying damage
        int plates{0};

        // Movement inaccuracy dynamics (deg/sec), per-entity type
        float move_spread_inc_rate_deg_per_sec_at_base{8.0f}; // per 350u/s equivalent
        float move_spread_decay_deg_per_sec{10.0f};
        float move_spread_max_deg{20.0f};
    } stats{};

    // Equipment
    std::optional<VID> equipped_gun_vid{};
    // Accuracy: accumulated movement spread (deg), rises with movement, decays at rest
    float move_spread_deg{0.0f};
};
