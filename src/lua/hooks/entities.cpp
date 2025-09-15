#include "luamgr.hpp"
#include "lua/lua_helpers.hpp"
#include "lua/internal_state.hpp"
#include <sol/sol.hpp>
#include "globals.hpp"
// hooks: entities

void LuaManager::call_entity_on_step(int entity_type, Entity& e) {
    const EntityTypeDef* ed = find_entity_type(entity_type);
    auto it = hooks_->entities.find(entity_type);
    if (!ed || it == hooks_->entities.end() || !it->second.on_step.valid()) return;
    LuaCtxGuard _ctx(ss, &e);
    auto r = it->second.on_step(); if (!r.valid()) { sol::error er = r; std::fprintf(stderr, "[lua] entity on_step error: %s\n", er.what()); }
}

void LuaManager::call_entity_on_damage(int entity_type, Entity& e, int attacker_ap) {
    const EntityTypeDef* ed = find_entity_type(entity_type);
    auto it2 = hooks_->entities.find(entity_type);
    if (!ed || it2 == hooks_->entities.end() || !it2->second.on_damage.valid()) return;
    LuaCtxGuard _ctx(ss, &e);
    auto r = it2->second.on_damage(attacker_ap); if (!r.valid()) { sol::error er = r; std::fprintf(stderr, "[lua] entity on_damage error: %s\n", er.what()); }
}

void LuaManager::call_entity_on_spawn(int entity_type, Entity& e) {
    const EntityTypeDef* ed = find_entity_type(entity_type);
    auto it3 = hooks_->entities.find(entity_type);
    if (!ed || it3 == hooks_->entities.end() || !it3->second.on_spawn.valid()) return;
    LuaCtxGuard _ctx(ss, &e);
    auto r = it3->second.on_spawn(); if (!r.valid()) { sol::error er = r; std::fprintf(stderr, "[lua] entity on_spawn error: %s\n", er.what()); }
}

void LuaManager::call_entity_on_death(int entity_type, Entity& e) {
    const EntityTypeDef* ed = find_entity_type(entity_type);
    auto it4 = hooks_->entities.find(entity_type);
    if (!ed || it4 == hooks_->entities.end() || !it4->second.on_death.valid()) return;
    LuaCtxGuard _ctx(ss, &e);
    auto r = it4->second.on_death(); if (!r.valid()) { sol::error er = r; std::fprintf(stderr, "[lua] entity on_death error: %s\n", er.what()); }
}

void LuaManager::call_entity_on_reload_start(int entity_type, Entity& e) {
    const EntityTypeDef* ed = find_entity_type(entity_type);
    auto it5 = hooks_->entities.find(entity_type);
    if (!ed || it5 == hooks_->entities.end() || !it5->second.on_reload_start.valid()) return;
    LuaCtxGuard _ctx(ss, &e);
    auto r = it5->second.on_reload_start(); if (!r.valid()) { sol::error er = r; std::fprintf(stderr, "[lua] entity on_reload_start error: %s\n", er.what()); }
}

void LuaManager::call_entity_on_reload_finish(int entity_type, Entity& e) {
    const EntityTypeDef* ed = find_entity_type(entity_type);
    auto it6 = hooks_->entities.find(entity_type);
    if (!ed || it6 == hooks_->entities.end() || !it6->second.on_reload_finish.valid()) return;
    LuaCtxGuard _ctx(ss, &e);
    auto r = it6->second.on_reload_finish(); if (!r.valid()) { sol::error er = r; std::fprintf(stderr, "[lua] entity on_reload_finish error: %s\n", er.what()); }
}

void LuaManager::call_entity_on_gun_jam(int entity_type, Entity& e) {
    const EntityTypeDef* ed = find_entity_type(entity_type);
    auto it7 = hooks_->entities.find(entity_type);
    if (!ed || it7 == hooks_->entities.end() || !it7->second.on_gun_jam.valid()) return;
    LuaCtxGuard _ctx(ss, &e);
    auto r = it7->second.on_gun_jam(); if (!r.valid()) { sol::error er = r; std::fprintf(stderr, "[lua] entity on_gun_jam error: %s\n", er.what()); }
}

void LuaManager::call_entity_on_out_of_ammo(int entity_type, Entity& e) {
    const EntityTypeDef* ed = find_entity_type(entity_type);
    auto it8 = hooks_->entities.find(entity_type);
    if (!ed || it8 == hooks_->entities.end() || !it8->second.on_out_of_ammo.valid()) return;
    LuaCtxGuard _ctx(ss, &e);
    auto r = it8->second.on_out_of_ammo(); if (!r.valid()) { sol::error er = r; std::fprintf(stderr, "[lua] entity on_out_of_ammo error: %s\n", er.what()); }
}

void LuaManager::call_entity_on_hp_under_50(int entity_type, Entity& e) {
    const EntityTypeDef* ed = find_entity_type(entity_type);
    auto it9 = hooks_->entities.find(entity_type);
    if (!ed || it9 == hooks_->entities.end() || !it9->second.on_hp_under_50.valid()) return;
    LuaCtxGuard _ctx(ss, &e);
    auto r = it9->second.on_hp_under_50(); if (!r.valid()) { sol::error er = r; std::fprintf(stderr, "[lua] entity on_hp_under_50 error: %s\n", er.what()); }
}

void LuaManager::call_entity_on_hp_under_25(int entity_type, Entity& e) {
    const EntityTypeDef* ed = find_entity_type(entity_type);
    auto it10 = hooks_->entities.find(entity_type);
    if (!ed || it10 == hooks_->entities.end() || !it10->second.on_hp_under_25.valid()) return;
    LuaCtxGuard _ctx(ss, &e);
    auto r = it10->second.on_hp_under_25(); if (!r.valid()) { sol::error er = r; std::fprintf(stderr, "[lua] entity on_hp_under_25 error: %s\n", er.what()); }
}

void LuaManager::call_entity_on_hp_full(int entity_type, Entity& e) {
    const EntityTypeDef* ed = find_entity_type(entity_type);
    auto it11 = hooks_->entities.find(entity_type);
    if (!ed || it11 == hooks_->entities.end() || !it11->second.on_hp_full.valid()) return;
    LuaCtxGuard _ctx(ss, &e);
    auto r = it11->second.on_hp_full(); if (!r.valid()) { sol::error er = r; std::fprintf(stderr, "[lua] entity on_hp_full error: %s\n", er.what()); }
}

void LuaManager::call_entity_on_shield_under_50(int entity_type, Entity& e) {
    const EntityTypeDef* ed = find_entity_type(entity_type);
    auto it12 = hooks_->entities.find(entity_type);
    if (!ed || it12 == hooks_->entities.end() || !it12->second.on_shield_under_50.valid()) return;
    LuaCtxGuard _ctx(ss, &e);
    auto r = it12->second.on_shield_under_50(); if (!r.valid()) { sol::error er = r; std::fprintf(stderr, "[lua] entity on_shield_under_50 error: %s\n", er.what()); }
}

void LuaManager::call_entity_on_shield_under_25(int entity_type, Entity& e) {
    const EntityTypeDef* ed = find_entity_type(entity_type);
    auto it13 = hooks_->entities.find(entity_type);
    if (!ed || it13 == hooks_->entities.end() || !it13->second.on_shield_under_25.valid()) return;
    LuaCtxGuard _ctx(ss, &e);
    auto r = it13->second.on_shield_under_25(); if (!r.valid()) { sol::error er = r; std::fprintf(stderr, "[lua] entity on_shield_under_25 error: %s\n", er.what()); }
}

void LuaManager::call_entity_on_shield_full(int entity_type, Entity& e) {
    const EntityTypeDef* ed = find_entity_type(entity_type);
    auto it14 = hooks_->entities.find(entity_type);
    if (!ed || it14 == hooks_->entities.end() || !it14->second.on_shield_full.valid()) return;
    LuaCtxGuard _ctx(ss, &e);
    auto r = it14->second.on_shield_full(); if (!r.valid()) { sol::error er = r; std::fprintf(stderr, "[lua] entity on_shield_full error: %s\n", er.what()); }
}

void LuaManager::call_entity_on_plates_lost(int entity_type, Entity& e) {
    const EntityTypeDef* ed = find_entity_type(entity_type);
    auto it15 = hooks_->entities.find(entity_type);
    if (!ed || it15 == hooks_->entities.end() || !it15->second.on_plates_lost.valid()) return;
    LuaCtxGuard _ctx(ss, &e);
    auto r = it15->second.on_plates_lost(); if (!r.valid()) { sol::error er = r; std::fprintf(stderr, "[lua] entity on_plates_lost error: %s\n", er.what()); }
}

void LuaManager::call_entity_on_collide_tile(int entity_type, Entity& e) {
    const EntityTypeDef* ed = find_entity_type(entity_type);
    auto it16 = hooks_->entities.find(entity_type);
    if (!ed || it16 == hooks_->entities.end() || !it16->second.on_collide_tile.valid()) return;
    LuaCtxGuard _ctx(ss, &e);
    auto r = it16->second.on_collide_tile(); if (!r.valid()) { sol::error er = r; std::fprintf(stderr, "[lua] entity on_collide_tile error: %s\n", er.what()); }
}
