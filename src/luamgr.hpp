#pragma once

#include <string>
#include <vector>
#include "lua/lua_defs.hpp"

struct State;
struct Entity;
namespace sol { struct state; }
struct lua_State; // forward-declare C Lua state; provided by sol2 at runtime

struct LuaManager {
  public:
    LuaManager();
    ~LuaManager();
    bool available() const;
    bool init();
    bool load_mods();
    // Allow registration helpers to access internals without exposing sol types
    friend void lua_register_powerups(sol::state& s, LuaManager& m);
    friend void lua_register_items(sol::state& s, LuaManager& m);
    friend void lua_register_guns(sol::state& s, LuaManager& m);
    friend void lua_register_ammo(sol::state& s, LuaManager& m);
    friend void lua_register_projectiles(sol::state& s, LuaManager& m);
    friend void lua_register_crates(sol::state& s, LuaManager& m);
    friend void lua_register_entities(sol::state& s, LuaManager& m);
    // Trigger calls
    bool call_item_on_use(int item_type, struct Entity& player, std::string* out_msg);
    void call_item_on_tick(int item_type, struct Entity& player, float dt);
    void call_item_on_shoot(int item_type, struct Entity& player);
    void call_item_on_damage(int item_type, struct Entity& player, int attacker_ap);
    void call_gun_on_jam(int gun_type, struct Entity& player);
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
    void call_crate_on_open(int crate_type, struct Entity& player);
    // Ammo hook calls
    void call_ammo_on_hit(int ammo_type);
    void call_ammo_on_hit_entity(int ammo_type);
    void call_ammo_on_hit_tile(int ammo_type);
    void call_on_dash(struct Entity& player);
    void call_on_step(struct Entity* player);
    void call_on_active_reload(struct Entity& player);
    void call_gun_on_active_reload(int gun_type, struct Entity& player);
    void call_item_on_active_reload(int item_type, struct Entity& player);
    void call_on_failed_active_reload(struct Entity& player);
    void call_gun_on_failed_active_reload(int gun_type, struct Entity& player);
    void call_item_on_failed_active_reload(int item_type, struct Entity& player);
    void call_on_tried_after_failed_ar(struct Entity& player);
    void call_gun_on_tried_after_failed_ar(int gun_type, struct Entity& player);
    void call_item_on_tried_after_failed_ar(int item_type, struct Entity& player);
    void call_gun_on_step(int gun_type, struct Entity& player);
    void call_gun_on_pickup(int gun_type, struct Entity& player);
    void call_gun_on_drop(int gun_type, struct Entity& player);
    void call_item_on_pickup(int item_type, struct Entity& player);
    void call_item_on_drop(int item_type, struct Entity& player);
    void call_on_eject(struct Entity& player);
    void call_gun_on_eject(int gun_type, struct Entity& player);
    void call_item_on_eject(int item_type, struct Entity& player);
    void call_on_reload_start(struct Entity& player);
    void call_gun_on_reload_start(int gun_type, struct Entity& player);
    void call_item_on_reload_start(int item_type, struct Entity& player);
    void call_on_reload_finish(struct Entity& player);
    void call_gun_on_reload_finish(int gun_type, struct Entity& player);
    void call_item_on_reload_finish(int item_type, struct Entity& player);
    const CrateDef* find_crate(int type) const {
        for (auto const& c : crates_)
            if (c.type == type)
                return &c;
        return nullptr;
    }
    void call_generate_room();
    void call_entity_on_step(int entity_type, struct Entity& e);
    void call_entity_on_damage(int entity_type, struct Entity& e, int attacker_ap);
    void call_entity_on_spawn(int entity_type, struct Entity& e);
    void call_entity_on_death(int entity_type, struct Entity& e);
    void call_entity_on_reload_start(int entity_type, struct Entity& e);
    void call_entity_on_reload_finish(int entity_type, struct Entity& e);
    void call_entity_on_gun_jam(int entity_type, struct Entity& e);
    void call_entity_on_out_of_ammo(int entity_type, struct Entity& e);
    void call_entity_on_hp_under_50(int entity_type, struct Entity& e);
    void call_entity_on_hp_under_25(int entity_type, struct Entity& e);
    void call_entity_on_hp_full(int entity_type, struct Entity& e);
    void call_entity_on_shield_under_50(int entity_type, struct Entity& e);
    void call_entity_on_shield_under_25(int entity_type, struct Entity& e);
    void call_entity_on_shield_full(int entity_type, struct Entity& e);
    void call_entity_on_plates_lost(int entity_type, struct Entity& e);
    void call_entity_on_collide_tile(int entity_type, struct Entity& e);

    // Query for optional hooks (used by engine tick schedulers)
    bool has_gun_on_step(int gun_type) const;
    bool has_item_on_tick(int item_type) const;
    bool has_entity_on_step(int entity_type) const;

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
    const std::vector<EntityTypeDef>& entity_types() const { return entity_types_; }
    const EntityTypeDef* find_entity_type(int type) const {
        for (auto const& e : entity_types_) {
            if (e.type == type)
                return &e;
        }
        return nullptr;
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
    void add_entity_type(const EntityTypeDef& d) { entity_types_.push_back(d); }

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
    std::vector<EntityTypeDef> entity_types_;
    DropTables drops_;

    lua_State* L{nullptr};
    sol::state* S{nullptr};
    struct LuaHooks* hooks_{nullptr};
};
