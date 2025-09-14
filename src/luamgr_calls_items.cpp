#include "luamgr.hpp"
#include "lua/lua_helpers.hpp"
#include "globals.hpp"

bool LuaManager::call_item_on_use(int item_type, Entity& player, std::string* out_msg) {
    const ItemDef* def = nullptr;
    for (auto const& d : items_) if (d.type == item_type) { def = &d; break; }
    if (!def || !def->on_use.valid()) return false;
    LuaCtxGuard _ctx(ss, &player);
    auto r = def->on_use();
    if (!r.valid()) { sol::error e = r; std::fprintf(stderr, "[lua] on_use error: %s\n", e.what()); return false; }
    if (out_msg && r.return_count() >= 1) {
        sol::object o = r.get<sol::object>();
        if (o.is<std::string>()) *out_msg = o.as<std::string>();
    }
    return true;
}

void LuaManager::call_item_on_tick(int item_type, Entity& player, float dt) {
    const ItemDef* def = nullptr;
    for (auto const& d : items_) if (d.type == item_type) { def = &d; break; }
    if (!def || !def->on_tick.valid()) return;
    LuaCtxGuard _ctx(ss, &player);
    auto r = def->on_tick(dt);
    if (!r.valid()) { sol::error e = r; std::fprintf(stderr, "[lua] on_tick error: %s\n", e.what()); }
}

void LuaManager::call_item_on_shoot(int item_type, Entity& player) {
    const ItemDef* def = nullptr;
    for (auto const& d : items_) if (d.type == item_type) { def = &d; break; }
    if (!def || !def->on_shoot.valid()) return;
    LuaCtxGuard _ctx(ss, &player);
    auto r = def->on_shoot();
    if (!r.valid()) { sol::error e = r; std::fprintf(stderr, "[lua] on_shoot error: %s\n", e.what()); }
}

void LuaManager::call_item_on_damage(int item_type, Entity& player, int attacker_ap) {
    const ItemDef* def = nullptr;
    for (auto const& d : items_) if (d.type == item_type) { def = &d; break; }
    if (!def || !def->on_damage.valid()) return;
    LuaCtxGuard _ctx(ss, &player);
    auto r = def->on_damage(attacker_ap);
    if (!r.valid()) { sol::error e = r; std::fprintf(stderr, "[lua] on_damage error: %s\n", e.what()); }
}

void LuaManager::call_item_on_pickup(int item_type, Entity& player) {
    const ItemDef* def = nullptr; for (auto const& d : items_) if (d.type == item_type) { def = &d; break; }
    if (!def || !def->on_pickup.valid()) return;
    LuaCtxGuard _ctx(ss, &player);
    auto r = def->on_pickup(); if (!r.valid()) { sol::error e = r; std::fprintf(stderr, "[lua] item on_pickup error: %s\n", e.what()); }
}

void LuaManager::call_item_on_drop(int item_type, Entity& player) {
    const ItemDef* def = nullptr; for (auto const& d : items_) if (d.type == item_type) { def = &d; break; }
    if (!def || !def->on_drop.valid()) return;
    LuaCtxGuard _ctx(ss, &player);
    auto r = def->on_drop(); if (!r.valid()) { sol::error e = r; std::fprintf(stderr, "[lua] item on_drop error: %s\n", e.what()); }
}

void LuaManager::call_item_on_active_reload(int item_type, Entity& player) {
    const ItemDef* def = nullptr; for (auto const& d : items_) if (d.type == item_type) { def = &d; break; }
    if (!def || !def->on_active_reload.valid()) return;
    LuaCtxGuard _ctx(ss, &player);
    auto r = def->on_active_reload(); if (!r.valid()) { sol::error e = r; std::fprintf(stderr, "[lua] item on_active_reload error: %s\n", e.what()); }
}

void LuaManager::call_item_on_failed_active_reload(int item_type, Entity& player) {
    const ItemDef* def = nullptr; for (auto const& d : items_) if (d.type == item_type) { def = &d; break; }
    if (!def || !def->on_failed_active_reload.valid()) return;
    LuaCtxGuard _ctx(ss, &player);
    auto r = def->on_failed_active_reload(); if (!r.valid()) { sol::error e = r; std::fprintf(stderr, "[lua] item on_failed_active_reload error: %s\n", e.what()); }
}

void LuaManager::call_item_on_tried_after_failed_ar(int item_type, Entity& player) {
    const ItemDef* def = nullptr; for (auto const& d : items_) if (d.type == item_type) { def = &d; break; }
    if (!def || !def->on_tried_after_failed_ar.valid()) return;
    LuaCtxGuard _ctx(ss, &player);
    auto r = def->on_tried_after_failed_ar(); if (!r.valid()) { sol::error e = r; std::fprintf(stderr, "[lua] item on_tried_after_failed_ar error: %s\n", e.what()); }
}

void LuaManager::call_item_on_eject(int item_type, Entity& player) {
    const ItemDef* def = nullptr; for (auto const& d : items_) if (d.type == item_type) { def = &d; break; }
    if (!def || !def->on_eject.valid()) return;
    LuaCtxGuard _ctx(ss, &player);
    auto r = def->on_eject(); if (!r.valid()) { sol::error e = r; std::fprintf(stderr, "[lua] item on_eject error: %s\n", e.what()); }
}

void LuaManager::call_item_on_reload_start(int item_type, Entity& player) {
    const ItemDef* def = nullptr; for (auto const& d : items_) if (d.type == item_type) { def = &d; break; }
    if (!def || !def->on_reload_start.valid()) return;
    LuaCtxGuard _ctx(ss, &player);
    auto r = def->on_reload_start(); if (!r.valid()) { sol::error e = r; std::fprintf(stderr, "[lua] item on_reload_start error: %s\n", e.what()); }
}

void LuaManager::call_item_on_reload_finish(int item_type, Entity& player) {
    const ItemDef* def = nullptr; for (auto const& d : items_) if (d.type == item_type) { def = &d; break; }
    if (!def || !def->on_reload_finish.valid()) return;
    LuaCtxGuard _ctx(ss, &player);
    auto r = def->on_reload_finish(); if (!r.valid()) { sol::error e = r; std::fprintf(stderr, "[lua] item on_reload_finish error: %s\n", e.what()); }
}

