#pragma once

#include <vector>
#include <cstdint>
#include <glm/glm.hpp>
#include "luamgr.hpp"

struct GunInstance {
  bool active{false};
  int def_type{0}; // matches GunDef::type from Lua
  int current_mag{0};
  int ammo_reserve{0};
  float heat{0.0f};
};

class GunsPool {
public:
  static constexpr std::size_t MAX = 64;
  GunsPool() { items.resize(MAX); }
  int spawn_from_def(const GunDef& d) {
    for (std::size_t i=0;i<items.size();++i) if (!items[i].active) {
      items[i].active = true; items[i].def_type = d.type; items[i].current_mag = d.mag; items[i].ammo_reserve = d.ammo_max; items[i].heat = 0.0f; return (int)i; }
    return -1;
  }
  GunInstance* get_mut(int id) { if (id<0) return nullptr; std::size_t i=(std::size_t)id; if (i>=items.size()) return nullptr; return &items[i]; }
  const GunInstance* get(int id) const { if (id<0) return nullptr; std::size_t i=(std::size_t)id; if (i>=items.size()) return nullptr; return &items[i]; }
  std::vector<GunInstance> items;
};

struct GroundGun {
  bool active{false};
  int gun_inst_id{-1};
  glm::vec2 pos{0.0f, 0.0f};
  glm::vec2 size{0.25f, 0.25f};
  int sprite_id{-1};
};

class GroundGunsPool {
public:
  static constexpr std::size_t MAX = 64;
  GroundGunsPool() { items.resize(MAX); }
  GroundGun* spawn(int inst_id, glm::vec2 p, int sprite_id) {
    for (auto& g : items) if (!g.active) { g.active=true; g.gun_inst_id=inst_id; g.pos=p; g.sprite_id=sprite_id; return &g; }
    return nullptr;
  }
  void clear() { for (auto& g : items) g.active=false; }
  std::vector<GroundGun>& data() { return items; }
  const std::vector<GroundGun>& data() const { return items; }
private:
  std::vector<GroundGun> items;
};

