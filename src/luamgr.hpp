#pragma once

#include <string>
#include <vector>

struct State;
struct Entity;
#ifdef GUB_USE_SOL2
namespace sol { class state; }
#endif

struct PowerupDef { std::string name; int type=0; std::string sprite; };
struct ItemDef {
  std::string name; int type=0; int category=0; int max_count=1; bool consume_on_use=false; std::string sprite; std::string desc;
  int on_use_ref{-1}; // Lua registry ref to on_use function (optional)
  int on_tick_ref{-1};
  int on_shoot_ref{-1};
  int on_damage_ref{-1};
};
struct GunDef {
  std::string name; int type=0; float damage=0.0f; float rpm=0.0f; float recoil=0.0f; float control=0.0f; int mag=0; int ammo_max=0;
};

struct DropEntry { int type{0}; float weight{1.0f}; };
struct DropTables { std::vector<DropEntry> powerups; std::vector<DropEntry> items; std::vector<DropEntry> guns; };

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

  const std::vector<PowerupDef>& powerups() const { return powerups_; }
  const std::vector<ItemDef>& items() const { return items_; }
  const std::vector<GunDef>& guns() const { return guns_; }
  const DropTables& drops() const { return drops_; }

  // Registration (used by Lua bindings)
  void add_powerup(const PowerupDef& d) { powerups_.push_back(d); }
  void add_item(const ItemDef& d) { items_.push_back(d); }
  void add_gun(const GunDef& d) { guns_.push_back(d); }

private:
  void clear();
  bool register_api();
  bool run_file(const std::string& path);

  std::vector<PowerupDef> powerups_;
  std::vector<ItemDef> items_;
  std::vector<GunDef> guns_;
  DropTables drops_;

#ifdef GUB_ENABLE_LUA
  struct lua_State* L{nullptr};
#else
  void* L{nullptr};
#endif
#ifdef GUB_USE_SOL2
  sol::state* S{nullptr};
#endif
};
