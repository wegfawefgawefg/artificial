#pragma once

#include <string>
#include <vector>

struct State;
struct Entity;
#ifdef GUB_USE_SOL2
namespace sol {
class state;
}
#endif

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
    int on_use_ref{-1}; // Lua registry ref to on_use function (optional)
    int on_tick_ref{-1};
    int on_shoot_ref{-1};
    int on_damage_ref{-1};
    int on_active_reload_ref{-1};
    int on_failed_active_reload_ref{-1};
    int on_tried_after_failed_ar_ref{-1};
    int on_pickup_ref{-1};
    int on_drop_ref{-1};
    int on_eject_ref{-1};
    int on_reload_start_ref{-1};
    int on_reload_finish_ref{-1};
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
    int on_jam_ref{-1};     // optional Lua callback
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
    int on_active_reload_ref{-1};
    int on_failed_active_reload_ref{-1};
    int on_tried_after_failed_ar_ref{-1};
    int on_pickup_ref{-1};
    int on_drop_ref{-1};
    int on_step_ref{-1};
    // Optional ticking (opt-in)
    float tick_rate_hz{0.0f};
    std::string tick_phase; // "before" or "after" (default after)
    int on_eject_ref{-1};
    int on_reload_start_ref{-1};
    int on_reload_finish_ref{-1};
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
    int on_hit_entity_ref{-1};
    int on_hit_tile_ref{-1};
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
    // Optional hooks
    int on_hit_ref{-1};
    int on_hit_entity_ref{-1};
    int on_hit_tile_ref{-1};
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
    int on_open_ref{-1};
};

class LuaManager {
  public:
    LuaManager();
    ~LuaManager();
    bool available() const;
    bool init();
    bool load_mods(const std::string& mods_root);
    // Trigger calls
    bool call_item_on_use(int item_type, State& state, struct Entity& player, std::string* out_msg);
    void call_item_on_tick(int item_type, State& state, struct Entity& player, float dt);
    void call_item_on_shoot(int item_type, State& state, struct Entity& player);
    void call_item_on_damage(int item_type, State& state, struct Entity& player, int attacker_ap);
    void call_gun_on_jam(int gun_type, State& state, struct Entity& player);
    const ProjectileDef* find_projectile(int type) const {
        for (auto const& p : projectiles_)
            if (p.type == type)
                return &p;
        return nullptr;
    }
    const GunDef* find_gun(int type) const {
        for (auto const& g : guns_)
            if (g.type == type)
                return &g;
        return nullptr;
    }
    void call_projectile_on_hit_entity(int proj_type);
    void call_projectile_on_hit_tile(int proj_type);
    void call_crate_on_open(int crate_type, State& state, struct Entity& player);
    // Ammo hook calls
    void call_ammo_on_hit(int ammo_type);
    void call_ammo_on_hit_entity(int ammo_type);
    void call_ammo_on_hit_tile(int ammo_type);
    void call_on_dash(State& state, struct Entity& player);
    void call_on_step(State& state, struct Entity* player);
    void call_on_active_reload(State& state, struct Entity& player);
    void call_gun_on_active_reload(int gun_type, State& state, struct Entity& player);
    void call_item_on_active_reload(int item_type, State& state, struct Entity& player);
    void call_on_failed_active_reload(State& state, struct Entity& player);
    void call_gun_on_failed_active_reload(int gun_type, State& state, struct Entity& player);
    void call_item_on_failed_active_reload(int item_type, State& state, struct Entity& player);
    void call_on_tried_after_failed_ar(State& state, struct Entity& player);
    void call_gun_on_tried_after_failed_ar(int gun_type, State& state, struct Entity& player);
    void call_item_on_tried_after_failed_ar(int item_type, State& state, struct Entity& player);
    void call_gun_on_step(int gun_type, State& state, struct Entity& player);
    void call_gun_on_pickup(int gun_type, State& state, struct Entity& player);
    void call_gun_on_drop(int gun_type, State& state, struct Entity& player);
    void call_item_on_pickup(int item_type, State& state, struct Entity& player);
    void call_item_on_drop(int item_type, State& state, struct Entity& player);
    void call_on_eject(State& state, struct Entity& player);
    void call_gun_on_eject(int gun_type, State& state, struct Entity& player);
    void call_item_on_eject(int item_type, State& state, struct Entity& player);
    void call_on_reload_start(State& state, struct Entity& player);
    void call_gun_on_reload_start(int gun_type, State& state, struct Entity& player);
    void call_item_on_reload_start(int item_type, State& state, struct Entity& player);
    void call_on_reload_finish(State& state, struct Entity& player);
    void call_gun_on_reload_finish(int gun_type, State& state, struct Entity& player);
    void call_item_on_reload_finish(int item_type, State& state, struct Entity& player);
    const CrateDef* find_crate(int type) const {
        for (auto const& c : crates_)
            if (c.type == type)
                return &c;
        return nullptr;
    }
    void call_generate_room(State& state);

    const std::vector<PowerupDef>& powerups() const {
        return powerups_;
    }
    const std::vector<ItemDef>& items() const {
        return items_;
    }
    const std::vector<GunDef>& guns() const {
        return guns_;
    }
    const std::vector<ProjectileDef>& projectiles() const {
        return projectiles_;
    }
    const DropTables& drops() const {
        return drops_;
    }
    const std::vector<AmmoDef>& ammo() const { return ammo_; }
    const AmmoDef* find_ammo(int type) const {
        for (auto const& a : ammo_)
            if (a.type == type)
                return &a;
        return nullptr;
    }
    const std::vector<CrateDef>& crates() const {
        return crates_;
    }

    // Registration (used by Lua bindings)
    void add_powerup(const PowerupDef& d) {
        powerups_.push_back(d);
    }
    void add_item(const ItemDef& d) {
        items_.push_back(d);
    }
    void add_gun(const GunDef& d) {
        guns_.push_back(d);
    }
    void add_projectile(const ProjectileDef& d) {
        projectiles_.push_back(d);
    }
    void add_ammo(const AmmoDef& d) { ammo_.push_back(d); }
    void add_crate(const CrateDef& d) {
        crates_.push_back(d);
    }

  private:
    void clear();
    bool register_api();
    bool run_file(const std::string& path);

    std::vector<PowerupDef> powerups_;
    std::vector<ItemDef> items_;
    std::vector<GunDef> guns_;
    std::vector<ProjectileDef> projectiles_;
    std::vector<AmmoDef> ammo_;
    std::vector<CrateDef> crates_;
    DropTables drops_;
    int on_dash_ref{-1};
    int on_active_reload_ref{-1};
    int on_failed_active_reload_ref{-1};
    int on_tried_after_failed_ar_ref{-1};
    int on_step_ref{-1};
    int on_eject_ref{-1};
    int on_reload_start_ref{-1};
    int on_reload_finish_ref{-1};

#ifdef GUB_ENABLE_LUA
    struct lua_State* L{nullptr};
#else
    void* L{nullptr};
#endif
#ifdef GUB_USE_SOL2
    sol::state* S{nullptr};
#endif
};
