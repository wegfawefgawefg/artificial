// Projectile stepping and hit effects implementation.
#include "globals.hpp"
#include "state.hpp"
#include "luamgr.hpp"
#include "projectiles.hpp"
#include "room.hpp"

#include <algorithm>
#include <cmath>
#include <random>

void step_projectiles_and_hits() {
    struct HitInfo {
        std::size_t eid; std::optional<VID> owner; float base_damage; float armor_pen; float shield_mult; int ammo_type; float travel_dist; int proj_def_type;
    };
    std::vector<HitInfo> hits;
    ss->projectiles.step(
        TIMESTEP, ss->stage, ss->entities.data(),
        [&](Projectile& pr, const Entity& hit) -> bool {
            if (luam && pr.def_type) luam->call_projectile_on_hit_entity(pr.def_type);
            if (luam && pr.ammo_type) luam->call_ammo_on_hit_entity(pr.ammo_type), luam->call_ammo_on_hit(pr.ammo_type);
            if (pr.owner) { if (auto* pm = ss->metrics_for(*pr.owner)) pm->shots_hit += 1; }
            hits.push_back(HitInfo{hit.vid.id, pr.owner, pr.base_damage, pr.armor_pen, pr.shield_mult, pr.ammo_type, pr.distance_travelled, pr.def_type});
            bool stop = true; if (pr.pierce_remaining > 0) { pr.pierce_remaining -= 1; stop = false; } return stop;
        },
        [&](Projectile& pr) {
            if (luam && pr.def_type) luam->call_projectile_on_hit_tile(pr.def_type);
            if (luam && pr.ammo_type) luam->call_ammo_on_hit_tile(pr.ammo_type), luam->call_ammo_on_hit(pr.ammo_type);
        }
    );

    for (auto h : hits) {
        auto id = h.eid; if (id >= ss->entities.data().size()) continue; auto& e = ss->entities.data()[id]; if (!e.active) continue;
        if (e.type_ == ids::ET_NPC || e.type_ == ids::ET_PLAYER) {
            if (e.health == 0) e.health = 3;
            if (e.max_hp == 0) e.max_hp = 3;
            float dmg = h.base_damage; float ap = std::clamp(h.armor_pen * 100.0f, 0.0f, 100.0f); float shield_mult = h.shield_mult;
            if (luam && h.ammo_type != 0) {
                if (auto const* ad = luam->find_ammo(h.ammo_type)) {
                    if (ad->falloff_end > ad->falloff_start && ad->falloff_end > 0.0f) {
                        float m = 1.0f;
                        if (h.travel_dist <= ad->falloff_start) m = 1.0f;
                        else if (h.travel_dist >= ad->falloff_end) m = ad->falloff_min_mult;
                        else { float t = (h.travel_dist - ad->falloff_start) / (ad->falloff_end - ad->falloff_start); m = 1.0f + t * (ad->falloff_min_mult - 1.0f); }
                        if (m < 0.0f) m = 0.0f;
                        dmg *= m;
                    }
                }
            }
            if (dmg <= 0.0f) dmg = 1.0f;
            if (e.type_ == ids::ET_PLAYER) {
                if (e.stats.shield_max > 0.0f && e.shield > 0.0f) {
                    float took = std::min(e.shield, (float)(dmg * shield_mult)); e.shield -= took;
                    if (auto* pm = ss->metrics_for(e.vid)) pm->damage_taken_shield += (std::uint64_t)std::lround(took);
                    if (h.owner) if (auto* om = ss->metrics_for(*h.owner)) om->damage_dealt += (std::uint64_t)std::lround(took);
                    dmg -= took; if (dmg < 0.0f) dmg = 0.0f;
                }
                if (dmg > 0.0f && e.stats.plates > 0) { e.stats.plates -= 1; if (auto* pm = ss->metrics_for(e.vid)) pm->plates_consumed += 1; if (e.stats.plates == 0 && luam && e.def_type) luam->call_entity_on_plates_lost(e.def_type, e); dmg = 0.0f; }
                if (luam && e.def_type) luam->call_entity_on_damage(e.def_type, e, (int)std::lround(ap));
                if (dmg > 0.0f) {
                    float reduction = std::max(0.0f, e.stats.armor - (float)ap); reduction = std::min(75.0f, reduction);
                    float scale = 1.0f - reduction * 0.01f; int delt = (int)std::ceil((double)dmg * (double)scale);
                    uint32_t before = e.health; uint32_t before_hp = e.health; e.health = (e.health > (uint32_t)delt) ? (e.health - (uint32_t)delt) : 0u;
                    if (auto* pm = ss->metrics_for(e.vid)) pm->damage_taken_hp += (std::uint64_t)(before - e.health);
                    if (h.owner) if (auto* om = ss->metrics_for(*h.owner)) om->damage_dealt += (std::uint64_t)delt;
                    if (luam && e.def_type && e.max_hp > 0) {
                        float prev = (float)before_hp / (float)e.max_hp; float now = (float)e.health / (float)e.max_hp;
                        if (prev >= 1.0f && now < 1.0f) { }
                        if (prev >= 0.5f && now < 0.5f) luam->call_entity_on_hp_under_50(e.def_type, e);
                        if (prev >= 0.25f && now < 0.25f) luam->call_entity_on_hp_under_25(e.def_type, e);
                        if (now <= 0.0f) { }
                    }
                }
            } else {
                if (e.stats.plates > 0) { e.stats.plates -= 1; if (e.stats.plates == 0 && luam && e.def_type) luam->call_entity_on_plates_lost(e.def_type, e); dmg = 0.0f; }
                if (luam && e.def_type) luam->call_entity_on_damage(e.def_type, e, (int)std::lround(ap));
                if (dmg > 0.0f) {
                    float reduction = std::max(0.0f, e.stats.armor - (float)ap); reduction = std::min(75.0f, reduction);
                    float scale = 1.0f - reduction * 0.01f; int delt = (int)std::ceil((double)dmg * (double)scale);
                    uint32_t before_hp = e.health; e.health = (e.health > (uint32_t)delt) ? (e.health - (uint32_t)delt) : 0u;
                    if (h.owner) if (auto* pm = ss->metrics_for(*h.owner)) pm->damage_dealt += (std::uint64_t)delt;
                    if (luam && e.def_type && e.max_hp > 0) {
                        float prev = (float)before_hp / (float)e.max_hp; float now = (float)e.health / (float)e.max_hp;
                        if (prev >= 0.5f && now < 0.5f) luam->call_entity_on_hp_under_50(e.def_type, e);
                        if (prev >= 0.25f && now < 0.25f) luam->call_entity_on_hp_under_25(e.def_type, e);
                    }
                }
            }
        }
        if (luam && e.def_type && e.max_hp > 0) {
            float now_hp = (float)e.health / (float)e.max_hp; if (e.last_hp_ratio < 1.0f && now_hp >= 1.0f) luam->call_entity_on_hp_full(e.def_type, e);
            e.last_hp_ratio = now_hp;
            if (e.stats.shield_max > 0.0f) {
                float now_sh = e.stats.shield_max > 0.0f ? (e.shield / e.stats.shield_max) : 0.0f;
                if (e.last_shield_ratio < 1.0f && now_sh >= 1.0f) luam->call_entity_on_shield_full(e.def_type, e);
                e.last_shield_ratio = now_sh;
            }
            if (e.last_plates < 0) e.last_plates = e.stats.plates;
        }
        e.time_since_damage = 0.0f;
        if (e.type_ == ids::ET_NPC && e.health == 0) {
            if (luam && e.def_type) luam->call_entity_on_death(e.def_type, e);
            glm::vec2 pos = e.pos; e.active = false; ss->metrics.enemies_slain += 1; ss->metrics.enemies_slain_by_type[(int)e.type_] += 1;
            if (h.owner) if (auto* pm = ss->metrics_for(*h.owner)) pm->enemies_slain += 1;
            static thread_local std::mt19937 rng{std::random_device{}()}; std::uniform_real_distribution<float> U(0.0f, 1.0f);
            if (U(rng) < 0.5f && luam) {
                glm::vec2 place_pos = ensure_not_in_block(pos);
                const auto& dt = luam->drops();
                auto pick_weighted = [&](const std::vector<DropEntry>& v) -> int {
                    if (v.empty()) return -1;
                    float sum = 0.0f;
                    for (auto const& de : v) sum += de.weight;
                    if (sum <= 0.0f) return -1;
                    std::uniform_real_distribution<float> du(0.0f, sum);
                    float r = du(rng), acc = 0.0f;
                    for (auto const& de : v) { acc += de.weight; if (r <= acc) return de.type; }
                    return v.back().type;
                };
                if (!dt.powerups.empty() || !dt.items.empty() || !dt.guns.empty()) {
                    float c = U(rng);
                    if (c < 0.5f && !dt.powerups.empty()) {
                        int t = pick_weighted(dt.powerups);
                        if (t >= 0) {
                            auto it = std::find_if(luam->powerups().begin(), luam->powerups().end(), [&](const PowerupDef& p) { return p.type == t; });
                            if (it != luam->powerups().end()) {
                                auto* p = ss->pickups.spawn((uint32_t)it->type, it->name, place_pos);
                                if (p) {
                                    ss->metrics.powerups_spawned += 1;
                                    if (!it->sprite.empty() && it->sprite.find(':') != std::string::npos) p->sprite_id = try_get_sprite_id(it->sprite); else p->sprite_id = -1;
                                }
                            }
                        }
                    } else if (c < 0.85f && !dt.items.empty()) {
                        int t = pick_weighted(dt.items);
                        if (t >= 0) {
                            auto it = std::find_if(luam->items().begin(), luam->items().end(), [&](const ItemDef& d) { return d.type == t; });
                            if (it != luam->items().end()) {
                                if (auto iv = ss->items.spawn_from_def(*it, 1)) { ss->ground_items.spawn(*iv, place_pos); ss->metrics.items_spawned += 1; }
                            }
                        }
                    } else if (!dt.guns.empty()) {
                        int t = pick_weighted(dt.guns);
                        if (t >= 0) {
                            auto ig = std::find_if(luam->guns().begin(), luam->guns().end(), [&](const GunDef& g) { return g.type == t; });
                            if (ig != luam->guns().end()) {
                                if (auto gv = ss->guns.spawn_from_def(*ig)) {
                                    int sid = -1; if (!ig->sprite.empty() && ig->sprite.find(':') != std::string::npos) sid = try_get_sprite_id(ig->sprite);
                                    ss->ground_guns.spawn(*gv, place_pos, sid); ss->metrics.guns_spawned += 1;
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
