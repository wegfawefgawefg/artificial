#pragma once

#include <string>
#include <vector>

struct State;
struct Entity;

struct PowerupDef { std::string name; int type=0; std::string sprite; };
struct ItemDef {
  std::string name; int type=0; int category=0; int max_count=1; bool consume_on_use=false; std::string sprite;
  int on_use_ref{-1}; // Lua registry ref to on_use function (optional)
};
struct GunDef {
  std::string name; int type=0; float damage=0.0f; float rpm=0.0f; float recoil=0.0f; float control=0.0f; int mag=0; int ammo_max=0;
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

  const std::vector<PowerupDef>& powerups() const { return powerups_; }
  const std::vector<ItemDef>& items() const { return items_; }
  const std::vector<GunDef>& guns() const { return guns_; }

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

#ifdef GUB_ENABLE_LUA
  struct lua_State* L{nullptr};
#else
  void* L{nullptr};
#endif
};
