#pragma once

#include <string>
#include <vector>

// Plain data structs used by mods. Keep sol types here so other headers
// don’t need to include sol directly.

struct PowerupDef {
    std::string name;
    int type = 0;
    std::string sprite;
};

struct ItemDef {
    std::string name;
    int type = 0;
    int category = 0;
    int max_count = 1;
    bool consume_on_use = false;
    std::string sprite;
    std::string desc;
    std::string sound_use;
    std::string sound_pickup;
    // Optional ticking (opt-in)
    float tick_rate_hz{0.0f};
    std::string tick_phase; // "before" or "after" (default after)
    // callbacks stored internally; not exposed here
};

struct AmmoCompat { int type{0}; float weight{1.0f}; };

struct GunDef {
    std::string name;
    int type = 0;
    float damage = 0.0f;
    float rpm = 0.0f;
    float deviation = 0.0f; // base angular deviation (degrees)
    float recoil = 0.0f;
    float control = 0.0f;
    float max_recoil_spread_deg{12.0f};
    int pellets_per_shot{1};
    int mag = 0;
    int ammo_max = 0;
    std::string sprite;
    std::string sound_fire;
    std::string sound_reload;
    std::string sound_jam;
    std::string sound_pickup;
    float jam_chance{0.0f}; // per-gun additive jam chance
    int projectile_type{0}; // projectile def to use
    std::string fire_mode;  // "auto", "single", or "burst"
    int burst_count{0};
    float burst_rpm{0.0f};
    // Optional explicit intervals (seconds). When 0, derived from RPM.
    float shot_interval{0.0f};
    float burst_interval{0.0f};
    float reload_time{1.0f};
    float eject_time{0.2f};
    // Active reload window definition
    float ar_pos{0.5f};               // center position 0..1
    float ar_pos_variance{0.0f};      // +/- variance applied to center
    float ar_size{0.15f};             // size 0..1
    float ar_size_variance{0.0f};     // +/- variance applied to size
    float active_reload_window{0.0f}; // legacy fallback for ar_size when >0
    // Optional ticking (opt-in)
    float tick_rate_hz{0.0f};
    std::string tick_phase; // "before" or "after" (default after)
    // Ammo compatibility (weighted pick on spawn)
    std::vector<AmmoCompat> compatible_ammo; // {type, weight}
};

struct ProjectileDef {
    std::string name;
    int type = 0;
    float speed = 20.0f;
    float size_x = 0.2f, size_y = 0.2f;
    int physics_steps = 2;
    std::string sprite; // namespaced sprite key (e.g., "mod:bullet")
    // callbacks stored internally; not exposed here
};

// Ammo definitions (Lua)
struct AmmoDef {
    std::string name;
    int type = 0;
    std::string desc;
    // Visuals / kinematics
    std::string sprite;   // namespaced sprite key for projectile
    float size_x{0.2f}, size_y{0.2f};
    float speed{20.0f};   // projectile speed (world units/sec)
    // Damage model
    float damage_mult{1.0f};   // scales gun base damage
    float armor_pen{0.0f};     // 0..1 fraction of armor ignored
    float shield_mult{1.0f};   // multiplier vs shields
    // Range / falloff
    float range_units{0.0f};        // 0 => unlimited
    float falloff_start{0.0f};      // distance where falloff begins
    float falloff_end{0.0f};        // distance where falloff reaches min
    float falloff_min_mult{1.0f};   // min damage multiplier at/after falloff_end
    int pierce_count{0};            // number of entities to pierce through
    // Optional hooks stored internally; not exposed here
};

inline const GunDef* find_gun_def_by_type(const std::vector<GunDef>& defs, int type) {
    for (auto const& g : defs) {
        if (g.type == type)
            return &g;
    }
    return nullptr;
}

struct DropEntry {
    int type{0};
    float weight{1.0f};
};
struct DropTables {
    std::vector<DropEntry> powerups;
    std::vector<DropEntry> items;
    std::vector<DropEntry> guns;
};

struct CrateDef {
    std::string name;
    int type = 0;
    float open_time = 5.0f;
    std::string label;
    DropTables drops;
    // on_open stored internally; not exposed here
};

// Entity type definitions (Lua)
struct EntityTypeDef {
    std::string name;
    int type = 0;
    std::string sprite;   // namespaced sprite key
    // Sizes in world units
    float sprite_w{0.25f};
    float sprite_h{0.25f};
    float collider_w{0.25f};
    float collider_h{0.25f};
    int physics_steps{1};
    // Core stats
    uint32_t max_hp{1000};
    float health_regen{0.0f};
    float shield_max{0.0f};
    float shield_regen{0.0f};
    float armor{0.0f};
    int plates{0};
    // Movement/accuracy
    float move_speed{350.0f};
    float dodge{3.0f};
    float accuracy{100.0f};
    float move_spread_inc_rate_deg_per_sec_at_base{8.0f};
    float move_spread_decay_deg_per_sec{10.0f};
    float move_spread_max_deg{20.0f};
    // Economy/modifiers
    float scavenging{100.0f};
    float currency{100.0f};
    float ammo_gain{100.0f};
    float luck{100.0f};
    // Combat
    float crit_chance{3.0f};
    float crit_damage{200.0f};
    float headshot_damage{200.0f};
    float damage_absorb{100.0f};
    float damage_output{100.0f};
    float healing{100.0f};
    float terror_level{100.0f};
    // Optional ticking
    float tick_rate_hz{0.0f};
    std::string tick_phase; // "before" or "after" (default after)
    // All callbacks stored internally; not exposed here
};
