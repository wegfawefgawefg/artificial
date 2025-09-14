#include "luamgr.hpp"

void LuaManager::call_gun_on_jam(int gun_type, Entity& player) {
    (void)player;
    const GunDef* gd = find_gun_def_by_type(guns_, gun_type);
    if (!gd || !gd->on_jam.valid()) return;
    auto r = gd->on_jam(); if (!r.valid()) { sol::error e = r; std::fprintf(stderr, "[lua] on_jam error: %s\n", e.what()); }
}

void LuaManager::call_gun_on_step(int gun_type, Entity& player) {
    (void)player;
    const GunDef* gd = find_gun(gun_type);
    if (!gd || !gd->on_step.valid()) return;
    auto r = gd->on_step(); if (!r.valid()) { sol::error e = r; std::fprintf(stderr, "[lua] gun on_step error: %s\n", e.what()); }
}

void LuaManager::call_gun_on_pickup(int gun_type, Entity& player) {
    (void)player;
    const GunDef* gd = find_gun_def_by_type(guns_, gun_type);
    if (!gd || !gd->on_pickup.valid()) return;
    auto r = gd->on_pickup(); if (!r.valid()) { sol::error e = r; std::fprintf(stderr, "[lua] gun on_pickup error: %s\n", e.what()); }
}

void LuaManager::call_gun_on_drop(int gun_type, Entity& player) {
    (void)player;
    const GunDef* gd = find_gun_def_by_type(guns_, gun_type);
    if (!gd || !gd->on_drop.valid()) return;
    auto r = gd->on_drop(); if (!r.valid()) { sol::error e = r; std::fprintf(stderr, "[lua] gun on_drop error: %s\n", e.what()); }
}

void LuaManager::call_gun_on_active_reload(int gun_type, Entity& player) {
    (void)player;
    const GunDef* gd = find_gun_def_by_type(guns_, gun_type);
    if (!gd || !gd->on_active_reload.valid()) return;
    auto r = gd->on_active_reload(); if (!r.valid()) { sol::error e = r; std::fprintf(stderr, "[lua] gun on_active_reload error: %s\n", e.what()); }
}

void LuaManager::call_gun_on_failed_active_reload(int gun_type, Entity& player) {
    (void)player;
    const GunDef* gd = find_gun_def_by_type(guns_, gun_type);
    if (!gd || !gd->on_failed_active_reload.valid()) return;
    auto r = gd->on_failed_active_reload(); if (!r.valid()) { sol::error e = r; std::fprintf(stderr, "[lua] gun on_failed_active_reload error: %s\n", e.what()); }
}

void LuaManager::call_gun_on_tried_after_failed_ar(int gun_type, Entity& player) {
    (void)player;
    const GunDef* gd = find_gun_def_by_type(guns_, gun_type);
    if (!gd || !gd->on_tried_after_failed_ar.valid()) return;
    auto r = gd->on_tried_after_failed_ar(); if (!r.valid()) { sol::error e = r; std::fprintf(stderr, "[lua] gun on_tried_after_failed_ar error: %s\n", e.what()); }
}

void LuaManager::call_gun_on_eject(int gun_type, Entity& player) {
    (void)player;
    const GunDef* gd = find_gun_def_by_type(guns_, gun_type);
    if (!gd || !gd->on_eject.valid()) return;
    auto r = gd->on_eject(); if (!r.valid()) { sol::error e = r; std::fprintf(stderr, "[lua] gun on_eject error: %s\n", e.what()); }
}

void LuaManager::call_gun_on_reload_start(int gun_type, Entity& player) {
    (void)player;
    const GunDef* gd = find_gun_def_by_type(guns_, gun_type);
    if (!gd || !gd->on_reload_start.valid()) return;
    auto r = gd->on_reload_start(); if (!r.valid()) { sol::error e = r; std::fprintf(stderr, "[lua] gun on_reload_start error: %s\n", e.what()); }
}

void LuaManager::call_gun_on_reload_finish(int gun_type, Entity& player) {
    (void)player;
    const GunDef* gd = find_gun_def_by_type(guns_, gun_type);
    if (!gd || !gd->on_reload_finish.valid()) return;
    auto r = gd->on_reload_finish(); if (!r.valid()) { sol::error e = r; std::fprintf(stderr, "[lua] gun on_reload_finish error: %s\n", e.what()); }
}

