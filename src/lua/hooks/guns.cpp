#include "luamgr.hpp"
// hooks: guns
#include "lua/internal_state.hpp"
#include <sol/sol.hpp>

void LuaManager::call_gun_on_jam(int gun_type, Entity& player) {
    (void)player;
    auto it = hooks_->guns.find(gun_type);
    if (it == hooks_->guns.end() || !it->second.on_jam.valid()) return;
    auto r = it->second.on_jam(); if (!r.valid()) { sol::error e = r; std::fprintf(stderr, "[lua] on_jam error: %s\n", e.what()); }
}

void LuaManager::call_gun_on_step(int gun_type, Entity& player) {
    (void)player;
    auto it2 = hooks_->guns.find(gun_type);
    if (it2 == hooks_->guns.end() || !it2->second.on_step.valid()) return;
    auto r = it2->second.on_step(); if (!r.valid()) { sol::error e = r; std::fprintf(stderr, "[lua] gun on_step error: %s\n", e.what()); }
}

void LuaManager::call_gun_on_pickup(int gun_type, Entity& player) {
    (void)player;
    auto it3 = hooks_->guns.find(gun_type);
    if (it3 == hooks_->guns.end() || !it3->second.on_pickup.valid()) return;
    auto r = it3->second.on_pickup(); if (!r.valid()) { sol::error e = r; std::fprintf(stderr, "[lua] gun on_pickup error: %s\n", e.what()); }
}

void LuaManager::call_gun_on_drop(int gun_type, Entity& player) {
    (void)player;
    auto it4 = hooks_->guns.find(gun_type);
    if (it4 == hooks_->guns.end() || !it4->second.on_drop.valid()) return;
    auto r = it4->second.on_drop(); if (!r.valid()) { sol::error e = r; std::fprintf(stderr, "[lua] gun on_drop error: %s\n", e.what()); }
}

void LuaManager::call_gun_on_active_reload(int gun_type, Entity& player) {
    (void)player;
    auto it5 = hooks_->guns.find(gun_type);
    if (it5 == hooks_->guns.end() || !it5->second.on_active_reload.valid()) return;
    auto r = it5->second.on_active_reload(); if (!r.valid()) { sol::error e = r; std::fprintf(stderr, "[lua] gun on_active_reload error: %s\n", e.what()); }
}

void LuaManager::call_gun_on_failed_active_reload(int gun_type, Entity& player) {
    (void)player;
    auto it6 = hooks_->guns.find(gun_type);
    if (it6 == hooks_->guns.end() || !it6->second.on_failed_active_reload.valid()) return;
    auto r = it6->second.on_failed_active_reload(); if (!r.valid()) { sol::error e = r; std::fprintf(stderr, "[lua] gun on_failed_active_reload error: %s\n", e.what()); }
}

void LuaManager::call_gun_on_tried_after_failed_ar(int gun_type, Entity& player) {
    (void)player;
    auto it7 = hooks_->guns.find(gun_type);
    if (it7 == hooks_->guns.end() || !it7->second.on_tried_after_failed_ar.valid()) return;
    auto r = it7->second.on_tried_after_failed_ar(); if (!r.valid()) { sol::error e = r; std::fprintf(stderr, "[lua] gun on_tried_after_failed_ar error: %s\n", e.what()); }
}

void LuaManager::call_gun_on_eject(int gun_type, Entity& player) {
    (void)player;
    auto it8 = hooks_->guns.find(gun_type);
    if (it8 == hooks_->guns.end() || !it8->second.on_eject.valid()) return;
    auto r = it8->second.on_eject(); if (!r.valid()) { sol::error e = r; std::fprintf(stderr, "[lua] gun on_eject error: %s\n", e.what()); }
}

void LuaManager::call_gun_on_reload_start(int gun_type, Entity& player) {
    (void)player;
    auto it9 = hooks_->guns.find(gun_type);
    if (it9 == hooks_->guns.end() || !it9->second.on_reload_start.valid()) return;
    auto r = it9->second.on_reload_start(); if (!r.valid()) { sol::error e = r; std::fprintf(stderr, "[lua] gun on_reload_start error: %s\n", e.what()); }
}

void LuaManager::call_gun_on_reload_finish(int gun_type, Entity& player) {
    (void)player;
    auto it10 = hooks_->guns.find(gun_type);
    if (it10 == hooks_->guns.end() || !it10->second.on_reload_finish.valid()) return;
    auto r = it10->second.on_reload_finish(); if (!r.valid()) { sol::error e = r; std::fprintf(stderr, "[lua] gun on_reload_finish error: %s\n", e.what()); }
}
