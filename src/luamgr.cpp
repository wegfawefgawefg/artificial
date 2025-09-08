#include "luamgr.hpp"
#include "state.hpp"
#include "entity.hpp"
#include <filesystem>
#include <cstdio>

#ifdef GUB_ENABLE_LUA
extern "C" {
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}
#endif

namespace fs = std::filesystem;

LuaManager::LuaManager() {}
LuaManager::~LuaManager() {
#ifdef GUB_ENABLE_LUA
  if (L) { lua_close(L); L=nullptr; }
#endif
}

bool LuaManager::available() const {
#ifdef GUB_ENABLE_LUA
  return true;
#else
  return false;
#endif
}

void LuaManager::clear() { powerups_.clear(); items_.clear(); guns_.clear(); }

bool LuaManager::init() {
#ifdef GUB_ENABLE_LUA
  if (L) return true;
  L = luaL_newstate();
  if (!L) return false;
  luaL_openlibs(L);
  return register_api();
#else
  return false;
#endif
}

#ifdef GUB_ENABLE_LUA
static LuaManager* g_mgr = nullptr;
static State* g_state_ctx = nullptr;
static Entity* g_player_ctx = nullptr;

static int l_api_add_plate(lua_State* Ls) {
  int n = 1; if (lua_gettop(Ls)>=1 && lua_isnumber(Ls,1)) n = (int)lua_tointeger(Ls,1);
  if (g_player_ctx) { g_player_ctx->stats.plates += n; if (g_player_ctx->stats.plates < 0) g_player_ctx->stats.plates = 0; }
  return 0;
}
static int l_api_heal(lua_State* Ls) {
  int n = 0; if (lua_gettop(Ls)>=1 && lua_isnumber(Ls,1)) n = (int)lua_tointeger(Ls,1);
  if (g_player_ctx) {
    std::uint32_t hp = g_player_ctx->health; std::uint32_t mx = g_player_ctx->max_hp; if (mx==0) mx=1000;
    std::uint32_t nhp = hp + (n>0 ? (std::uint32_t)n : 0u); if (nhp > mx) nhp = mx; g_player_ctx->health = nhp;
  }
  return 0;
}
static int l_api_add_move_speed(lua_State* Ls) {
  int n = 0; if (lua_gettop(Ls)>=1 && lua_isnumber(Ls,1)) n = (int)lua_tointeger(Ls,1);
  if (g_player_ctx) { g_player_ctx->stats.move_speed += (float)n; }
  return 0;
}

static int l_register_powerup(lua_State* L) {
  if (!lua_istable(L, 1)) return 0;
  PowerupDef d{};
  lua_getfield(L, 1, "name"); if (lua_isstring(L, -1)) d.name = lua_tostring(L, -1); lua_pop(L,1);
  lua_getfield(L, 1, "type"); if (lua_isinteger(L, -1)) d.type = (int)lua_tointeger(L, -1); lua_pop(L,1);
  lua_getfield(L, 1, "sprite"); if (lua_isstring(L, -1)) d.sprite = lua_tostring(L, -1); lua_pop(L,1);
  if (g_mgr) g_mgr->add_powerup(d);
  return 0;
}
static int l_register_item(lua_State* L) {
  if (!lua_istable(L, 1)) return 0;
  ItemDef d{};
  lua_getfield(L, 1, "name"); if (lua_isstring(L, -1)) d.name = lua_tostring(L, -1); lua_pop(L,1);
  lua_getfield(L, 1, "type"); if (lua_isinteger(L, -1)) d.type = (int)lua_tointeger(L, -1); lua_pop(L,1);
  lua_getfield(L, 1, "category"); if (lua_isinteger(L, -1)) d.category = (int)lua_tointeger(L, -1); lua_pop(L,1);
  lua_getfield(L, 1, "max_count"); if (lua_isinteger(L, -1)) d.max_count = (int)lua_tointeger(L, -1); lua_pop(L,1);
  lua_getfield(L, 1, "consume_on_use"); if (lua_isboolean(L, -1)) d.consume_on_use = lua_toboolean(L, -1)!=0; lua_pop(L,1);
  lua_getfield(L, 1, "sprite"); if (lua_isstring(L, -1)) d.sprite = lua_tostring(L, -1); lua_pop(L,1);
  // optional on_use callback
  int on_use_ref = -1;
  lua_getfield(L, 1, "on_use");
  if (lua_isfunction(L, -1)) { on_use_ref = luaL_ref(L, LUA_REGISTRYINDEX); }
  else { lua_pop(L,1); }
  d.on_use_ref = on_use_ref;
  if (g_mgr) g_mgr->add_item(d);
  return 0;
}
static int l_register_gun(lua_State* L) {
  if (!lua_istable(L, 1)) return 0;
  GunDef d{};
  lua_getfield(L, 1, "name"); if (lua_isstring(L, -1)) d.name = lua_tostring(L, -1); lua_pop(L,1);
  lua_getfield(L, 1, "type"); if (lua_isinteger(L, -1)) d.type = (int)lua_tointeger(L, -1); lua_pop(L,1);
  lua_getfield(L, 1, "damage"); if (lua_isnumber(L, -1)) d.damage = (float)lua_tonumber(L, -1); lua_pop(L,1);
  lua_getfield(L, 1, "rpm"); if (lua_isnumber(L, -1)) d.rpm = (float)lua_tonumber(L, -1); lua_pop(L,1);
  lua_getfield(L, 1, "recoil"); if (lua_isnumber(L, -1)) d.recoil = (float)lua_tonumber(L, -1); lua_pop(L,1);
  lua_getfield(L, 1, "control"); if (lua_isnumber(L, -1)) d.control = (float)lua_tonumber(L, -1); lua_pop(L,1);
  lua_getfield(L, 1, "mag"); if (lua_isinteger(L, -1)) d.mag = (int)lua_tointeger(L, -1); lua_pop(L,1);
  lua_getfield(L, 1, "ammo_max"); if (lua_isinteger(L, -1)) d.ammo_max = (int)lua_tointeger(L, -1); lua_pop(L,1);
  if (g_mgr) g_mgr->add_gun(d);
  return 0;
}
#endif

bool LuaManager::register_api() {
#ifdef GUB_ENABLE_LUA
  g_mgr = this;
  lua_pushcfunction(L, l_register_powerup); lua_setglobal(L, "register_powerup");
  lua_pushcfunction(L, l_register_item); lua_setglobal(L, "register_item");
  lua_pushcfunction(L, l_register_gun); lua_setglobal(L, "register_gun");
  // expose api table
  lua_newtable(L);
  lua_pushcfunction(L, l_api_add_plate); lua_setfield(L, -2, "add_plate");
  lua_pushcfunction(L, l_api_heal); lua_setfield(L, -2, "heal");
  lua_pushcfunction(L, l_api_add_move_speed); lua_setfield(L, -2, "add_move_speed");
  lua_setglobal(L, "api");
  return true;
#else
  return false;
#endif
}

bool LuaManager::run_file(const std::string& path) {
#ifdef GUB_ENABLE_LUA
  if (luaL_dofile(L, path.c_str()) != LUA_OK) {
    const char* err = lua_tostring(L, -1);
    std::fprintf(stderr, "[lua] error in %s: %s\n", path.c_str(), err?err:"(unknown)");
    lua_pop(L, 1);
    return false;
  }
  return true;
#else
  (void)path; return false;
#endif
}

bool LuaManager::load_mods(const std::string& mods_root) {
  clear();
#ifdef GUB_ENABLE_LUA
  std::error_code ec;
  if (!fs::exists(mods_root, ec) || !fs::is_directory(mods_root, ec)) return false;
  for (auto const& mod : fs::directory_iterator(mods_root, ec)) {
    if (ec) { ec.clear(); continue; }
    if (!mod.is_directory()) continue;
    fs::path sdir = mod.path() / "scripts";
    if (fs::exists(sdir, ec) && fs::is_directory(sdir, ec)) {
      for (auto const& f : fs::directory_iterator(sdir, ec)) {
        if (ec) { ec.clear(); continue; }
        if (!f.is_regular_file()) continue;
        auto p = f.path();
        if (p.extension()==".lua") run_file(p.string());
      }
    }
  }
  std::printf("[lua] loaded: %zu powerups, %zu items, %zu guns\n", powerups_.size(), items_.size(), guns_.size());
  return true;
#else
  (void)mods_root; return false;
#endif
}

bool LuaManager::call_item_on_use(int item_type, State& state, Entity& player, std::string* out_msg) {
#ifdef GUB_ENABLE_LUA
  const ItemDef* def = nullptr; for (auto const& d : items_) if (d.type == item_type) { def = &d; break; }
  if (!def || def->on_use_ref < 0) return false;
  g_state_ctx = &state; g_player_ctx = &player;
  lua_rawgeti(L, LUA_REGISTRYINDEX, def->on_use_ref);
  if (lua_pcall(L, 0, 1, 0) != LUA_OK) {
    const char* err = lua_tostring(L, -1);
    std::fprintf(stderr, "[lua] on_use error: %s\n", err?err:"(unknown)");
    lua_pop(L,1);
    g_state_ctx=nullptr; g_player_ctx=nullptr;
    return false;
  }
  if (out_msg && lua_isstring(L, -1)) { *out_msg = lua_tostring(L, -1); }
  lua_pop(L,1);
  g_state_ctx=nullptr; g_player_ctx=nullptr;
  return true;
#else
  (void)item_type; (void)state; (void)player; (void)out_msg; return false;
#endif
}
