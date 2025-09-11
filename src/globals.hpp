#pragma once

struct LuaManager;
struct SpriteIdRegistry;
struct Graphics;

// Shared pointers to systems used across modules.
extern LuaManager* g_lua_mgr;
extern SpriteIdRegistry* g_sprite_ids;
extern Graphics* g_gfx;
