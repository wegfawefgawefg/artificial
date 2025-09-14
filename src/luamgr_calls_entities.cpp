#include "luamgr.hpp"
#include "lua/lua_helpers.hpp"
#include "globals.hpp"

void LuaManager::call_entity_on_step(int entity_type, Entity& e) {
    const EntityTypeDef* ed = find_entity_type(entity_type);
    if (!ed || !ed->on_step.valid()) return;
    LuaCtxGuard _ctx(ss, &e);
    auto r = ed->on_step(); if (!r.valid()) { sol::error er = r; std::fprintf(stderr, "[lua] entity on_step error: %s\n", er.what()); }
}

void LuaManager::call_entity_on_damage(int entity_type, Entity& e, int attacker_ap) {
    const EntityTypeDef* ed = find_entity_type(entity_type);
    if (!ed || !ed->on_damage.valid()) return;
    LuaCtxGuard _ctx(ss, &e);
    auto r = ed->on_damage(attacker_ap); if (!r.valid()) { sol::error er = r; std::fprintf(stderr, "[lua] entity on_damage error: %s\n", er.what()); }
}

void LuaManager::call_entity_on_spawn(int entity_type, Entity& e) {
    const EntityTypeDef* ed = find_entity_type(entity_type);
    if (!ed || !ed->on_spawn.valid()) return;
    LuaCtxGuard _ctx(ss, &e);
    auto r = ed->on_spawn(); if (!r.valid()) { sol::error er = r; std::fprintf(stderr, "[lua] entity on_spawn error: %s\n", er.what()); }
}

void LuaManager::call_entity_on_death(int entity_type, Entity& e) {
    const EntityTypeDef* ed = find_entity_type(entity_type);
    if (!ed || !ed->on_death.valid()) return;
    LuaCtxGuard _ctx(ss, &e);
    auto r = ed->on_death(); if (!r.valid()) { sol::error er = r; std::fprintf(stderr, "[lua] entity on_death error: %s\n", er.what()); }
}

void LuaManager::call_entity_on_reload_start(int entity_type, Entity& e) {
    const EntityTypeDef* ed = find_entity_type(entity_type);
    if (!ed || !ed->on_reload_start.valid()) return;
    LuaCtxGuard _ctx(ss, &e);
    auto r = ed->on_reload_start(); if (!r.valid()) { sol::error er = r; std::fprintf(stderr, "[lua] entity on_reload_start error: %s\n", er.what()); }
}

void LuaManager::call_entity_on_reload_finish(int entity_type, Entity& e) {
    const EntityTypeDef* ed = find_entity_type(entity_type);
    if (!ed || !ed->on_reload_finish.valid()) return;
    LuaCtxGuard _ctx(ss, &e);
    auto r = ed->on_reload_finish(); if (!r.valid()) { sol::error er = r; std::fprintf(stderr, "[lua] entity on_reload_finish error: %s\n", er.what()); }
}

void LuaManager::call_entity_on_gun_jam(int entity_type, Entity& e) {
    const EntityTypeDef* ed = find_entity_type(entity_type);
    if (!ed || !ed->on_gun_jam.valid()) return;
    LuaCtxGuard _ctx(ss, &e);
    auto r = ed->on_gun_jam(); if (!r.valid()) { sol::error er = r; std::fprintf(stderr, "[lua] entity on_gun_jam error: %s\n", er.what()); }
}

void LuaManager::call_entity_on_out_of_ammo(int entity_type, Entity& e) {
    const EntityTypeDef* ed = find_entity_type(entity_type);
    if (!ed || !ed->on_out_of_ammo.valid()) return;
    LuaCtxGuard _ctx(ss, &e);
    auto r = ed->on_out_of_ammo(); if (!r.valid()) { sol::error er = r; std::fprintf(stderr, "[lua] entity on_out_of_ammo error: %s\n", er.what()); }
}

void LuaManager::call_entity_on_hp_under_50(int entity_type, Entity& e) {
    const EntityTypeDef* ed = find_entity_type(entity_type);
    if (!ed || !ed->on_hp_under_50.valid()) return;
    LuaCtxGuard _ctx(ss, &e);
    auto r = ed->on_hp_under_50(); if (!r.valid()) { sol::error er = r; std::fprintf(stderr, "[lua] entity on_hp_under_50 error: %s\n", er.what()); }
}

void LuaManager::call_entity_on_hp_under_25(int entity_type, Entity& e) {
    const EntityTypeDef* ed = find_entity_type(entity_type);
    if (!ed || !ed->on_hp_under_25.valid()) return;
    LuaCtxGuard _ctx(ss, &e);
    auto r = ed->on_hp_under_25(); if (!r.valid()) { sol::error er = r; std::fprintf(stderr, "[lua] entity on_hp_under_25 error: %s\n", er.what()); }
}

void LuaManager::call_entity_on_hp_full(int entity_type, Entity& e) {
    const EntityTypeDef* ed = find_entity_type(entity_type);
    if (!ed || !ed->on_hp_full.valid()) return;
    LuaCtxGuard _ctx(ss, &e);
    auto r = ed->on_hp_full(); if (!r.valid()) { sol::error er = r; std::fprintf(stderr, "[lua] entity on_hp_full error: %s\n", er.what()); }
}

void LuaManager::call_entity_on_shield_under_50(int entity_type, Entity& e) {
    const EntityTypeDef* ed = find_entity_type(entity_type);
    if (!ed || !ed->on_shield_under_50.valid()) return;
    LuaCtxGuard _ctx(ss, &e);
    auto r = ed->on_shield_under_50(); if (!r.valid()) { sol::error er = r; std::fprintf(stderr, "[lua] entity on_shield_under_50 error: %s\n", er.what()); }
}

void LuaManager::call_entity_on_shield_under_25(int entity_type, Entity& e) {
    const EntityTypeDef* ed = find_entity_type(entity_type);
    if (!ed || !ed->on_shield_under_25.valid()) return;
    LuaCtxGuard _ctx(ss, &e);
    auto r = ed->on_shield_under_25(); if (!r.valid()) { sol::error er = r; std::fprintf(stderr, "[lua] entity on_shield_under_25 error: %s\n", er.what()); }
}

void LuaManager::call_entity_on_shield_full(int entity_type, Entity& e) {
    const EntityTypeDef* ed = find_entity_type(entity_type);
    if (!ed || !ed->on_shield_full.valid()) return;
    LuaCtxGuard _ctx(ss, &e);
    auto r = ed->on_shield_full(); if (!r.valid()) { sol::error er = r; std::fprintf(stderr, "[lua] entity on_shield_full error: %s\n", er.what()); }
}

void LuaManager::call_entity_on_plates_lost(int entity_type, Entity& e) {
    const EntityTypeDef* ed = find_entity_type(entity_type);
    if (!ed || !ed->on_plates_lost.valid()) return;
    LuaCtxGuard _ctx(ss, &e);
    auto r = ed->on_plates_lost(); if (!r.valid()) { sol::error er = r; std::fprintf(stderr, "[lua] entity on_plates_lost error: %s\n", er.what()); }
}

void LuaManager::call_entity_on_collide_tile(int entity_type, Entity& e) {
    const EntityTypeDef* ed = find_entity_type(entity_type);
    if (!ed || !ed->on_collide_tile.valid()) return;
    LuaCtxGuard _ctx(ss, &e);
    auto r = ed->on_collide_tile(); if (!r.valid()) { sol::error er = r; std::fprintf(stderr, "[lua] entity on_collide_tile error: %s\n", er.what()); }
}
