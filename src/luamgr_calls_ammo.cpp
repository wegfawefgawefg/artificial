#include "luamgr.hpp"

void LuaManager::call_ammo_on_hit(int ammo_type) {
    const AmmoDef* ad = find_ammo(ammo_type);
    if (!ad || !ad->on_hit.valid()) return;
    auto r = ad->on_hit();
    if (!r.valid()) { sol::error e = r; std::fprintf(stderr, "[lua] ammo on_hit error: %s\n", e.what()); }
}

void LuaManager::call_ammo_on_hit_entity(int ammo_type) {
    const AmmoDef* ad = find_ammo(ammo_type);
    if (!ad || !ad->on_hit_entity.valid()) return;
    auto r = ad->on_hit_entity();
    if (!r.valid()) { sol::error e = r; std::fprintf(stderr, "[lua] ammo on_hit_entity error: %s\n", e.what()); }
}

void LuaManager::call_ammo_on_hit_tile(int ammo_type) {
    const AmmoDef* ad = find_ammo(ammo_type);
    if (!ad || !ad->on_hit_tile.valid()) return;
    auto r = ad->on_hit_tile();
    if (!r.valid()) { sol::error e = r; std::fprintf(stderr, "[lua] ammo on_hit_tile error: %s\n", e.what()); }
}

