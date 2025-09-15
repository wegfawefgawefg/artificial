#include "luamgr.hpp"
// hooks: global
#include "lua/lua_helpers.hpp"
#include "lua/internal_state.hpp"
#include "globals.hpp"
#include <sol/sol.hpp>

void LuaManager::call_on_dash(Entity& player) {
    LuaCtxGuard _ctx(ss, &player);
    if (hooks_->global.on_dash.valid()) {
        auto r = hooks_->global.on_dash(); if (!r.valid()) { sol::error e = r; std::fprintf(stderr, "[lua] on_dash error: %s\n", e.what()); }
    } else {
        sol::object obj = S->get<sol::object>("on_dash");
        if (obj.is<sol::function>()) { auto r = obj.as<sol::protected_function>()(); if (!r.valid()) { sol::error e = r; std::fprintf(stderr, "[lua] on_dash error: %s\n", e.what()); } }
    }
}

void LuaManager::call_on_step(Entity* player) {
    LuaCtxGuard _ctx(ss, player);
    if (hooks_->global.on_step.valid()) { auto r = hooks_->global.on_step(); if (!r.valid()) { sol::error e = r; std::fprintf(stderr, "[lua] on_step error: %s\n", e.what()); } }
}

void LuaManager::call_on_active_reload(Entity& player) {
    LuaCtxGuard _ctx(ss, &player);
    if (hooks_->global.on_active_reload.valid()) { auto r = hooks_->global.on_active_reload(); if (!r.valid()) { sol::error e = r; std::fprintf(stderr, "[lua] on_active_reload error: %s\n", e.what()); } }
    else {
        sol::object obj = S->get<sol::object>("on_active_reload");
        if (obj.is<sol::function>()) { auto r = obj.as<sol::protected_function>()(); if (!r.valid()) { sol::error e = r; std::fprintf(stderr, "[lua] on_active_reload error: %s\n", e.what()); } }
    }
}

void LuaManager::call_on_failed_active_reload(Entity& player) {
    LuaCtxGuard _ctx(ss, &player);
    if (hooks_->global.on_failed_active_reload.valid()) { auto r = hooks_->global.on_failed_active_reload(); if (!r.valid()) { sol::error e = r; std::fprintf(stderr, "[lua] on_failed_active_reload error: %s\n", e.what()); } }
    else {
        sol::object obj = S->get<sol::object>("on_failed_active_reload");
        if (obj.is<sol::function>()) { auto r = obj.as<sol::protected_function>()(); if (!r.valid()) { sol::error e = r; std::fprintf(stderr, "[lua] on_failed_active_reload error: %s\n", e.what()); } }
    }
}

void LuaManager::call_on_tried_after_failed_ar(Entity& player) {
    LuaCtxGuard _ctx(ss, &player);
    if (hooks_->global.on_tried_after_failed_ar.valid()) { auto r = hooks_->global.on_tried_after_failed_ar(); if (!r.valid()) { sol::error e = r; std::fprintf(stderr, "[lua] on_tried_to_active_reload_after_failing error: %s\n", e.what()); } }
    else {
        sol::object obj = S->get<sol::object>("on_tried_to_active_reload_after_failing");
        if (obj.is<sol::function>()) { auto r = obj.as<sol::protected_function>()(); if (!r.valid()) { sol::error e = r; std::fprintf(stderr, "[lua] on_tried_to_active_reload_after_failing error: %s\n", e.what()); } }
    }
}

void LuaManager::call_on_eject(Entity& player) {
    LuaCtxGuard _ctx(ss, &player);
    if (hooks_->global.on_eject.valid()) { auto r = hooks_->global.on_eject(); if (!r.valid()) { sol::error e = r; std::fprintf(stderr, "[lua] on_eject error: %s\n", e.what()); } }
    else {
        sol::object obj = S->get<sol::object>("on_eject");
        if (obj.is<sol::function>()) { auto r = obj.as<sol::protected_function>()(); if (!r.valid()) { sol::error e = r; std::fprintf(stderr, "[lua] on_eject error: %s\n", e.what()); } }
    }
}

void LuaManager::call_on_reload_start(Entity& player) {
    LuaCtxGuard _ctx(ss, &player);
    if (hooks_->global.on_reload_start.valid()) { auto r = hooks_->global.on_reload_start(); if (!r.valid()) { sol::error e = r; std::fprintf(stderr, "[lua] on_reload_start error: %s\n", e.what()); } }
    else {
        sol::object obj = S->get<sol::object>("on_reload_start");
        if (obj.is<sol::function>()) { auto r = obj.as<sol::protected_function>()(); if (!r.valid()) { sol::error e = r; std::fprintf(stderr, "[lua] on_reload_start error: %s\n", e.what()); } }
    }
}

void LuaManager::call_on_reload_finish(Entity& player) {
    LuaCtxGuard _ctx(ss, &player);
    if (hooks_->global.on_reload_finish.valid()) { auto r = hooks_->global.on_reload_finish(); if (!r.valid()) { sol::error e = r; std::fprintf(stderr, "[lua] on_reload_finish error: %s\n", e.what()); } }
    else {
        sol::object obj = S->get<sol::object>("on_reload_finish");
        if (obj.is<sol::function>()) { auto r = obj.as<sol::protected_function>()(); if (!r.valid()) { sol::error e = r; std::fprintf(stderr, "[lua] on_reload_finish error: %s\n", e.what()); } }
    }
}
