// Lua scripting tick execution (pre/post physics).
#include "globals.hpp"
#include "luamgr.hpp"

#include <algorithm>
#include <string>

void pre_physics_ticks() {
    if (ss->player_vid && luam) {
        Entity* plbt = ss->entities.get_mut(*ss->player_vid);
        if (plbt) {
            const float dt = TIMESTEP;
            const int MAX_TICKS = 4000;
            int tick_calls = 0;
            // Guns with on_step (before phase)
            const Inventory* pinv = (ss->player_vid ? ss->inv_for(*ss->player_vid) : nullptr);
            if (pinv)
            for (const auto& entry : pinv->entries) {
                if (entry.kind != INV_GUN) continue;
                GunInstance* gi = ss->guns.get(entry.vid);
                if (!gi) continue;
                const GunDef* gd = nullptr;
                for (auto const& g : luam->guns()) if (g.type == gi->def_type) { gd = &g; break; }
                if (!gd || !luam->has_gun_on_step(gd->type)) continue;
                if (gd->tick_rate_hz <= 0.0f || gd->tick_phase == std::string("after")) continue;
                gi->tick_acc += dt;
                float period = 1.0f / std::max(1.0f, gd->tick_rate_hz);
                while (gi->tick_acc >= period && tick_calls < MAX_TICKS) {
                    luam->call_gun_on_step(gi->def_type, *plbt);
                    gi->tick_acc -= period;
                    ++tick_calls;
                }
            }
            // Items with on_tick (before phase)
            if (pinv)
            for (const auto& entry : pinv->entries) {
                if (entry.kind != INV_ITEM) continue;
                ItemInstance* inst = ss->items.get(entry.vid);
                if (!inst) continue;
                const ItemDef* idf = nullptr;
                for (auto const& d : luam->items()) if (d.type == inst->def_type) { idf = &d; break; }
                if (!idf || !luam->has_item_on_tick(idf->type)) continue;
                if (idf->tick_rate_hz <= 0.0f || idf->tick_phase == std::string("after")) continue;
                inst->tick_acc += dt;
                float period = 1.0f / std::max(1.0f, idf->tick_rate_hz);
                while (inst->tick_acc >= period && tick_calls < MAX_TICKS) {
                    luam->call_item_on_tick(inst->def_type, *plbt, period);
                    inst->tick_acc -= period;
                    ++tick_calls;
                }
            }
        }
    }
    // Entity type on_step ticks (before phase)
    if (luam) {
        const float dt = TIMESTEP;
        const int MAX_TICKS = 4000;
        int tick_calls = 0;
        for (auto& e : ss->entities.data()) {
            if (!e.active || e.def_type == 0) continue;
            const auto* ed = luam->find_entity_type(e.def_type);
            if (!ed) continue;
            if (ed->tick_rate_hz <= 0.0f || ed->tick_phase == std::string("after") || !luam->has_entity_on_step(ed->type)) continue;
            e.tick_acc_entity += dt;
            float period = 1.0f / std::max(1.0f, ed->tick_rate_hz);
            while (e.tick_acc_entity >= period && tick_calls < MAX_TICKS) {
                luam->call_entity_on_step(e.def_type, e);
                e.tick_acc_entity -= period;
                ++tick_calls;
            }
        }
    }
}

void post_physics_ticks() {
    if (!ss) return;
    // Player inventory-driven hooks (after phase)
    Entity* plat = ss->player_vid ? ss->entities.get_mut(*ss->player_vid) : nullptr;
    if (plat && luam) {
        const float dt = TIMESTEP;
        const int MAX_TICKS = 4000;
        int tick_calls = 0;
        // Guns with on_step
        if (auto* inv = ss->inv_for(*ss->player_vid)) for (const auto& entry : inv->entries) {
            if (entry.kind != INV_GUN) continue;
            GunInstance* gi = ss->guns.get(entry.vid);
            if (!gi) continue;
            const GunDef* gd = nullptr;
            for (auto const& g : luam->guns()) if (g.type == gi->def_type) { gd = &g; break; }
            if (!gd || !luam->has_gun_on_step(gd->type)) continue;
            if (gd->tick_rate_hz <= 0.0f || gd->tick_phase == std::string("before")) continue;
            gi->tick_acc += dt;
            float period = 1.0f / std::max(1.0f, gd->tick_rate_hz);
            while (gi->tick_acc >= period && tick_calls < MAX_TICKS) {
                luam->call_gun_on_step(gi->def_type, *plat);
                gi->tick_acc -= period;
                ++tick_calls;
            }
        }
        // Items with on_tick
        if (auto* inv = ss->inv_for(*ss->player_vid)) for (const auto& entry : inv->entries) {
            if (entry.kind != INV_ITEM) continue;
            ItemInstance* inst = ss->items.get(entry.vid);
            if (!inst) continue;
            const ItemDef* idf = nullptr;
            for (auto const& d : luam->items()) if (d.type == inst->def_type) { idf = &d; break; }
            if (!idf || !luam->has_item_on_tick(idf->type)) continue;
            if (idf->tick_rate_hz <= 0.0f || idf->tick_phase == std::string("before")) continue;
            inst->tick_acc += dt;
            float period = 1.0f / std::max(1.0f, idf->tick_rate_hz);
            while (inst->tick_acc >= period && tick_calls < MAX_TICKS) {
                luam->call_item_on_tick(inst->def_type, *plat, period);
                inst->tick_acc -= period;
                ++tick_calls;
            }
        }
    }
    // Entity type on_step ticks (after phase)
    if (luam) {
        const int MAX_TICKS = 4000;
        int tick_calls = 0;
        for (auto& e : ss->entities.data()) {
            if (!e.active || e.def_type == 0) continue;
            const auto* ed = luam->find_entity_type(e.def_type);
            if (!ed) continue;
            if (ed->tick_rate_hz <= 0.0f || ed->tick_phase != std::string("after") || !luam->has_entity_on_step(ed->type)) continue;
            e.tick_acc_entity += TIMESTEP;
            float period = 1.0f / std::max(1.0f, ed->tick_rate_hz);
            while (e.tick_acc_entity >= period && tick_calls < MAX_TICKS) {
                luam->call_entity_on_step(e.def_type, e);
                e.tick_acc_entity -= period;
                ++tick_calls;
            }
        }
    }
}
