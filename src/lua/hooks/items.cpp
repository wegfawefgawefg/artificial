#include "luamgr.hpp"
// hooks: items
#include "lua/lua_helpers.hpp"
#include "lua/internal_state.hpp"
#include "globals.hpp"
#include <sol/sol.hpp>

bool LuaManager::call_item_on_use(int item_type, Entity& player, std::string* out_msg) {
    const ItemDef* def = nullptr;
    for (auto const& d : items_) if (d.type == item_type) { def = &d; break; }
    if (!def) return false;
    auto it = hooks_->items.find(item_type);
    if (it == hooks_->items.end() || !it->second.on_use.valid()) return false;
    LuaCtxGuard _ctx(ss, &player);
    auto r = it->second.on_use();
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
    if (!def) return;
    auto it = hooks_->items.find(item_type);
    if (it == hooks_->items.end() || !it->second.on_tick.valid()) return;
    LuaCtxGuard _ctx(ss, &player);
    auto r = it->second.on_tick(dt);
    if (!r.valid()) { sol::error e = r; std::fprintf(stderr, "[lua] on_tick error: %s\n", e.what()); }
}

void LuaManager::call_item_on_shoot(int item_type, Entity& player) {
    const ItemDef* def = nullptr;
    for (auto const& d : items_) if (d.type == item_type) { def = &d; break; }
    if (!def) return;
    auto it = hooks_->items.find(item_type);
    if (it == hooks_->items.end() || !it->second.on_shoot.valid()) return;
    LuaCtxGuard _ctx(ss, &player);
    auto r = it->second.on_shoot();
    if (!r.valid()) { sol::error e = r; std::fprintf(stderr, "[lua] on_shoot error: %s\n", e.what()); }
}

void LuaManager::call_item_on_damage(int item_type, Entity& player, int attacker_ap) {
    const ItemDef* def = nullptr;
    for (auto const& d : items_) if (d.type == item_type) { def = &d; break; }
    if (!def) return;
    auto it = hooks_->items.find(item_type);
    if (it == hooks_->items.end() || !it->second.on_damage.valid()) return;
    LuaCtxGuard _ctx(ss, &player);
        auto r = it->second.on_damage(attacker_ap);
    if (!r.valid()) { sol::error e = r; std::fprintf(stderr, "[lua] on_damage error: %s\n", e.what()); }
}

void LuaManager::call_item_on_pickup(int item_type, Entity& player) {
    const ItemDef* def = nullptr;
    for (auto const& d : items_) if (d.type == item_type) { def = &d; break; }
    auto it = hooks_->items.find(item_type);
    if (!def || it == hooks_->items.end() || !it->second.on_pickup.valid()) return;
    LuaCtxGuard _ctx(ss, &player);
    auto r = it->second.on_pickup(); if (!r.valid()) { sol::error e = r; std::fprintf(stderr, "[lua] item on_pickup error: %s\n", e.what()); }
}

void LuaManager::call_item_on_drop(int item_type, Entity& player) {
    const ItemDef* def = nullptr;
    for (auto const& d : items_) if (d.type == item_type) { def = &d; break; }
    auto it2 = hooks_->items.find(item_type);
    if (!def || it2 == hooks_->items.end() || !it2->second.on_drop.valid()) return;
    LuaCtxGuard _ctx(ss, &player);
    auto r = it2->second.on_drop(); if (!r.valid()) { sol::error e = r; std::fprintf(stderr, "[lua] item on_drop error: %s\n", e.what()); }
}

void LuaManager::call_item_on_active_reload(int item_type, Entity& player) {
    const ItemDef* def = nullptr;
    for (auto const& d : items_) if (d.type == item_type) { def = &d; break; }
    auto it3 = hooks_->items.find(item_type);
    if (!def || it3 == hooks_->items.end() || !it3->second.on_active_reload.valid()) return;
    LuaCtxGuard _ctx(ss, &player);
    auto r = it3->second.on_active_reload(); if (!r.valid()) { sol::error e = r; std::fprintf(stderr, "[lua] item on_active_reload error: %s\n", e.what()); }
}

void LuaManager::call_item_on_failed_active_reload(int item_type, Entity& player) {
    const ItemDef* def = nullptr;
    for (auto const& d : items_) if (d.type == item_type) { def = &d; break; }
    auto it4 = hooks_->items.find(item_type);
    if (!def || it4 == hooks_->items.end() || !it4->second.on_failed_active_reload.valid()) return;
    LuaCtxGuard _ctx(ss, &player);
    auto r = it4->second.on_failed_active_reload(); if (!r.valid()) { sol::error e = r; std::fprintf(stderr, "[lua] item on_failed_active_reload error: %s\n", e.what()); }
}

void LuaManager::call_item_on_tried_after_failed_ar(int item_type, Entity& player) {
    const ItemDef* def = nullptr;
    for (auto const& d : items_) if (d.type == item_type) { def = &d; break; }
    auto it5 = hooks_->items.find(item_type);
    if (!def || it5 == hooks_->items.end() || !it5->second.on_tried_after_failed_ar.valid()) return;
    LuaCtxGuard _ctx(ss, &player);
    auto r = it5->second.on_tried_after_failed_ar(); if (!r.valid()) { sol::error e = r; std::fprintf(stderr, "[lua] item on_tried_after_failed_ar error: %s\n", e.what()); }
}

void LuaManager::call_item_on_eject(int item_type, Entity& player) {
    const ItemDef* def = nullptr;
    for (auto const& d : items_) if (d.type == item_type) { def = &d; break; }
    auto it6 = hooks_->items.find(item_type);
    if (!def || it6 == hooks_->items.end() || !it6->second.on_eject.valid()) return;
    LuaCtxGuard _ctx(ss, &player);
    auto r = it6->second.on_eject(); if (!r.valid()) { sol::error e = r; std::fprintf(stderr, "[lua] item on_eject error: %s\n", e.what()); }
}

void LuaManager::call_item_on_reload_start(int item_type, Entity& player) {
    const ItemDef* def = nullptr;
    for (auto const& d : items_) if (d.type == item_type) { def = &d; break; }
    auto it7 = hooks_->items.find(item_type);
    if (!def || it7 == hooks_->items.end() || !it7->second.on_reload_start.valid()) return;
    LuaCtxGuard _ctx(ss, &player);
    auto r = it7->second.on_reload_start(); if (!r.valid()) { sol::error e = r; std::fprintf(stderr, "[lua] item on_reload_start error: %s\n", e.what()); }
}

void LuaManager::call_item_on_reload_finish(int item_type, Entity& player) {
    const ItemDef* def = nullptr;
    for (auto const& d : items_) if (d.type == item_type) { def = &d; break; }
    auto it8 = hooks_->items.find(item_type);
    if (!def || it8 == hooks_->items.end() || !it8->second.on_reload_finish.valid()) return;
    LuaCtxGuard _ctx(ss, &player);
    auto r = it8->second.on_reload_finish(); if (!r.valid()) { sol::error e = r; std::fprintf(stderr, "[lua] item on_reload_finish error: %s\n", e.what()); }
}
