#pragma once

struct LuaManager;
struct SpriteIdRegistry;
class SpriteStore;
struct Graphics;
struct Audio;
class ModsManager;
struct State;

// Shared pointers to systems used across modules.
extern LuaManager* g_lua_mgr;
extern SpriteIdRegistry* g_sprite_ids;
extern Graphics* g_gfx;
extern Audio* g_audio;
extern SpriteStore* g_sprite_store;
class TextureStore;
extern TextureStore* g_textures;
extern ModsManager* g_mods;
struct InputBindings;
struct InputContext;
extern InputBindings* g_binds;
extern InputContext* g_input;
struct RuntimeSettings;
extern RuntimeSettings* g_settings;
extern State* g_state;
