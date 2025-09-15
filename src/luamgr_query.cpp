#include "luamgr.hpp"
#include "lua/internal_state.hpp"
#include <sol/sol.hpp>

bool LuaManager::has_gun_on_step(int gun_type) const {
    if (!hooks_) return false;
    auto it = hooks_->guns.find(gun_type);
    return it != hooks_->guns.end() && it->second.on_step.valid();
}

bool LuaManager::has_item_on_tick(int item_type) const {
    if (!hooks_) return false;
    auto it = hooks_->items.find(item_type);
    return it != hooks_->items.end() && it->second.on_tick.valid();
}

bool LuaManager::has_entity_on_step(int entity_type) const {
    if (!hooks_) return false;
    auto it = hooks_->entities.find(entity_type);
    return it != hooks_->entities.end() && it->second.on_step.valid();
}

