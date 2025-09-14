#pragma once

struct State;
struct Entity;
class LuaManager;

// Global context bridge for Lua callbacks
extern LuaManager* g_mgr;
extern State* g_state_ctx;
extern Entity* g_player_ctx;

struct LuaCtxGuard {
    LuaCtxGuard(State* s, Entity* p);
    ~LuaCtxGuard();
};

