#include "luamgr.hpp"
// hooks: projectiles
#include "lua/internal_state.hpp"
#include <sol/sol.hpp>

void LuaManager::call_projectile_on_hit_entity(int proj_type) {
    auto it = hooks_->projectiles.find(proj_type);
    if (it == hooks_->projectiles.end() || !it->second.on_hit_entity.valid()) return;
    auto r = it->second.on_hit_entity();
    if (!r.valid()) { sol::error e = r; std::fprintf(stderr, "[lua] projectile on_hit_entity error: %s\n", e.what()); }
}

void LuaManager::call_projectile_on_hit_tile(int proj_type) {
    auto it2 = hooks_->projectiles.find(proj_type);
    if (it2 == hooks_->projectiles.end() || !it2->second.on_hit_tile.valid()) return;
    auto r = it2->second.on_hit_tile();
    if (!r.valid()) { sol::error e = r; std::fprintf(stderr, "[lua] projectile on_hit_tile error: %s\n", e.what()); }
}
