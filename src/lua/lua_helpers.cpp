#include "lua_helpers.hpp"

LuaManager* g_mgr = nullptr;
State* g_state_ctx = nullptr;
Entity* g_player_ctx = nullptr;

LuaCtxGuard::LuaCtxGuard(State* s, Entity* p) {
    g_state_ctx = s;
    g_player_ctx = p;
}

LuaCtxGuard::~LuaCtxGuard() {
    g_state_ctx = nullptr;
    g_player_ctx = nullptr;
}

