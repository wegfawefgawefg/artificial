#include "luamgr.hpp"

void LuaManager::call_projectile_on_hit_entity(int proj_type) {
    const ProjectileDef* pd = find_projectile(proj_type);
    if (!pd || !pd->on_hit_entity.valid()) return;
    auto r = pd->on_hit_entity();
    if (!r.valid()) { sol::error e = r; std::fprintf(stderr, "[lua] projectile on_hit_entity error: %s\n", e.what()); }
}

void LuaManager::call_projectile_on_hit_tile(int proj_type) {
    const ProjectileDef* pd = find_projectile(proj_type);
    if (!pd || !pd->on_hit_tile.valid()) return;
    auto r = pd->on_hit_tile();
    if (!r.valid()) { sol::error e = r; std::fprintf(stderr, "[lua] projectile on_hit_tile error: %s\n", e.what()); }
}

