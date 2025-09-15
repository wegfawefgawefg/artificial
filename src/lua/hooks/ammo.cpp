#include "luamgr.hpp"
// hooks: ammo
#include "lua/internal_state.hpp"
#include <sol/sol.hpp>

void LuaManager::call_ammo_on_hit(int ammo_type) {
    auto it = hooks_->ammo.find(ammo_type);
    if (it == hooks_->ammo.end() || !it->second.on_hit.valid()) return;
    auto r = it->second.on_hit();
    if (!r.valid()) { sol::error e = r; std::fprintf(stderr, "[lua] ammo on_hit error: %s\n", e.what()); }
}

void LuaManager::call_ammo_on_hit_entity(int ammo_type) {
    auto it2 = hooks_->ammo.find(ammo_type);
    if (it2 == hooks_->ammo.end() || !it2->second.on_hit_entity.valid()) return;
    auto r = it2->second.on_hit_entity();
    if (!r.valid()) { sol::error e = r; std::fprintf(stderr, "[lua] ammo on_hit_entity error: %s\n", e.what()); }
}

void LuaManager::call_ammo_on_hit_tile(int ammo_type) {
    auto it3 = hooks_->ammo.find(ammo_type);
    if (it3 == hooks_->ammo.end() || !it3->second.on_hit_tile.valid()) return;
    auto r = it3->second.on_hit_tile();
    if (!r.valid()) { sol::error e = r; std::fprintf(stderr, "[lua] ammo on_hit_tile error: %s\n", e.what()); }
}
