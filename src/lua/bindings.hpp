// bindings declarations (moved, bottled)
#pragma once

#include <sol/sol.hpp>

class LuaManager;

// Registration helpers for Lua API surface
void lua_register_powerups(sol::state& s, LuaManager& m);
void lua_register_items(sol::state& s, LuaManager& m);
void lua_register_guns(sol::state& s, LuaManager& m);
void lua_register_ammo(sol::state& s, LuaManager& m);
void lua_register_projectiles(sol::state& s, LuaManager& m);
void lua_register_crates(sol::state& s, LuaManager& m);
void lua_register_entities(sol::state& s, LuaManager& m);

void lua_register_api_player(sol::state& s, LuaManager& m);
void lua_register_api_world(sol::state& s, LuaManager& m);
