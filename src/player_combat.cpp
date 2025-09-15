// Player combat implementation: shields/reload, reload edges, firing, jam/unjam.
#include "globals.hpp"
#include "luamgr.hpp"
#include "audio.hpp"
#include "guns.hpp"
#include "items.hpp"
#include "settings.hpp"
#include "state.hpp"
#include "player_combat.hpp"

#include <algorithm>
#include <random>
#include <string>
#include <cmath>

void update_shields_and_reload_progress() {
    for (auto& e : ss->entities.data()) {
        if (!e.active) continue;
        if (e.stats.shield_max > 0.0f && e.time_since_damage >= 3.0f) {
            float prev_ratio = (e.stats.shield_max > 0.0f) ? (e.shield / e.stats.shield_max) : 0.0f;
            e.shield = std::min(e.stats.shield_max, e.shield + e.stats.shield_regen * TIMESTEP);
            float ratio = (e.stats.shield_max > 0.0f) ? (e.shield / e.stats.shield_max) : 0.0f;
            if (luam && e.def_type) {
                if (prev_ratio < 1.0f && ratio >= 1.0f) luam->call_entity_on_shield_full(e.def_type, e);
                if (prev_ratio >= 0.5f && ratio < 0.5f) luam->call_entity_on_shield_under_50(e.def_type, e);
                if (prev_ratio >= 0.25f && ratio < 0.25f) luam->call_entity_on_shield_under_25(e.def_type, e);
            }
            e.last_shield_ratio = ratio;
        }
        if (e.type_ == ids::ET_PLAYER && e.equipped_gun_vid.has_value()) {
            if (auto* gi = ss->guns.get(*e.equipped_gun_vid)) {
                if (gi->reloading) {
                    const GunDef* gd = nullptr;
                    if (luam) {
                        for (auto const& g : luam->guns()) if (g.type == gi->def_type) { gd = &g; break; }
                    }
                    if (gi->reload_eject_remaining > 0.0f) {
                        gi->reload_eject_remaining = std::max(0.0f, gi->reload_eject_remaining - TIMESTEP);
                    } else if (gi->reload_total_time > 0.0f) {
                        gi->reload_progress = std::min(1.0f, gi->reload_progress + (TIMESTEP / gi->reload_total_time));
                    }
                    if (gi->reload_progress >= 1.0f) {
                        if (gd && gi->ammo_reserve > 0) {
                            int take = std::min(gd->mag, gi->ammo_reserve);
                            gi->current_mag = take;
                            gi->ammo_reserve -= take;
                        }
                        gi->reloading = false;
                        gi->reload_progress = 0.0f;
                        gi->burst_remaining = 0;
                        gi->burst_timer = 0.0f;
                        if (luam && ss->player_vid) {
                            if (auto* plmm = ss->entities.get_mut(*ss->player_vid)) if (plmm->def_type)
                                luam->call_entity_on_reload_finish(plmm->def_type, *plmm);
                        }
                    }
                }
            }
        }
    }
}

void update_reload_active() {
    if (!ss || ss->mode != ids::MODE_PLAYING || !ss->player_vid) return;
    auto* plm = ss->entities.get_mut(*ss->player_vid);
    if (!plm || !plm->equipped_gun_vid.has_value()) return;
    static bool prev_reload = false;
    bool now_reload = ss->playing_inputs.reload;
    if (now_reload && !prev_reload) {
        GunInstance* gim = ss->guns.get(*plm->equipped_gun_vid);
        if (gim) {
            const GunDef* gd = nullptr;
            if (luam) {
                for (auto const& g : luam->guns()) if (g.type == gim->def_type) { gd = &g; break; }
            }
            if (gim->jammed) {
                ss->alerts.push_back({"Gun jammed! Mash SPACE", 0.0f, 1.2f, false});
                if (aa) play_sound("base:ui_cant");
            } else if (gd) {
                if (gim->reloading) {
                    float prog = gim->reload_progress;
                    if (gim->reload_total_time > 0.0f) prog = gim->reload_progress;
                    if (!gim->ar_consumed && prog >= gim->ar_window_start && prog <= gim->ar_window_end) {
                        int take = std::min(gd->mag, gim->ammo_reserve);
                        gim->current_mag = take;
                        gim->ammo_reserve -= take;
                        gim->reloading = false;
                        gim->reload_progress = 0.0f;
                        gim->burst_remaining = 0;
                        gim->burst_timer = 0.0f;
                        ss->alerts.push_back({"Active Reload!", 0.0f, 1.2f, false});
                        ss->reticle_shake = std::max(ss->reticle_shake, 6.0f);
                        if (aa) play_sound("base:ui_super_confirm");
                        if (ss->player_vid) if (auto* pm = ss->metrics_for(*ss->player_vid)) pm->active_reload_success += 1;
                        if (luam) {
                            luam->call_on_active_reload(*plm);
                            luam->call_gun_on_active_reload(gim->def_type, *plm);
                            if (auto* inv = (ss->player_vid ? ss->inv_for(*ss->player_vid) : nullptr)) for (const auto& entry : inv->entries) {
                                if (entry.kind == INV_ITEM) if (const ItemInstance* inst = ss->items.get(entry.vid)) luam->call_item_on_active_reload(inst->def_type, *plm);
                            }
                        }
                    } else if (!gim->ar_consumed) {
                        gim->ar_consumed = true;
                        gim->ar_failed_attempt = true;
                        ss->reload_bar_shake = std::max(ss->reload_bar_shake, 6.0f);
                        ss->alerts.push_back({"Active Reload Failed", 0.0f, 0.7f, false});
                        if (ss->player_vid) if (auto* pm = ss->metrics_for(*ss->player_vid)) pm->active_reload_fail += 1;
                        if (luam) {
                            luam->call_on_failed_active_reload(*plm);
                            luam->call_gun_on_failed_active_reload(gim->def_type, *plm);
                            if (auto* inv = (ss->player_vid ? ss->inv_for(*ss->player_vid) : nullptr)) for (const auto& entry : inv->entries) {
                                if (entry.kind == INV_ITEM) if (const ItemInstance* inst = ss->items.get(entry.vid)) luam->call_item_on_failed_active_reload(inst->def_type, *plm);
                            }
                        }
                    } else if (gim->ar_consumed && gim->ar_failed_attempt) {
                        if (luam) {
                            luam->call_on_tried_after_failed_ar(*plm);
                            luam->call_gun_on_tried_after_failed_ar(gim->def_type, *plm);
                            if (auto* inv = (ss->player_vid ? ss->inv_for(*ss->player_vid) : nullptr)) for (const auto& entry : inv->entries) {
                                if (entry.kind == INV_ITEM) if (const ItemInstance* inst = ss->items.get(entry.vid)) luam->call_item_on_tried_after_failed_ar(inst->def_type, *plm);
                            }
                        }
                    }
                } else if (gim->ammo_reserve > 0) {
                    int dropped = gim->current_mag;
                    if (dropped > 0) ss->alerts.push_back({std::string("Dropped ") + std::to_string(dropped) + " bullets", 0.0f, 1.0f, false});
                    gim->current_mag = 0;
                    gim->reloading = true;
                    if (plm->def_type && luam) luam->call_entity_on_reload_start(plm->def_type, *plm);
                    if (ss->player_vid) if (auto* pm = ss->metrics_for(*ss->player_vid)) pm->reloads += 1;
                    gim->reload_progress = 0.0f;
                    gim->reload_eject_remaining = std::max(0.0f, gd->eject_time);
                    gim->reload_total_time = std::max(0.1f, gd->reload_time);
                    gim->burst_remaining = 0;
                    gim->burst_timer = 0.0f;
                    static thread_local std::mt19937 rng{std::random_device{}()};
                    auto clamp01 = [](float v) { return std::max(0.0f, std::min(1.0f, v)); };
                    std::uniform_real_distribution<float> Upos(-gd->ar_pos_variance, gd->ar_pos_variance);
                    std::uniform_real_distribution<float> Usize(-gd->ar_size_variance, gd->ar_size_variance);
                    float size = std::clamp(gd->ar_size + Usize(rng), 0.02f, 0.9f);
                    float center = clamp01(gd->ar_pos + Upos(rng));
                    float start = clamp01(center - size * 0.5f);
                    if (start + size > 1.0f) start = 1.0f - size;
                    gim->ar_window_start = start;
                    gim->ar_window_end = start + size;
                    gim->ar_consumed = false;
                    gim->ar_failed_attempt = false;
                    if (aa) play_sound(gd->sound_reload.empty() ? "base:reload" : gd->sound_reload);
                } else {
                    ss->alerts.push_back({std::string("NO AMMO"), 0.0f, 1.5f, false});
                    if (plm->def_type && luam) luam->call_entity_on_out_of_ammo(plm->def_type, *plm);
                }
            }
        }
    }
    prev_reload = now_reload;
}

void update_trigger_and_fire() {
    if (!ss) return;
    ss->gun_cooldown = std::max(0.0f, ss->gun_cooldown - TIMESTEP);
    bool can_fire = (ss->gun_cooldown == 0.0f);
    static bool prev_shoot = false;
    bool trig_held = (ss->mode == ids::MODE_PLAYING) ? ss->mouse_inputs.left : false;
    bool trig_edge = trig_held && !prev_shoot;
    if (ss->mode == ids::MODE_PLAYING) prev_shoot = trig_held;
    bool fire_request = false;
    bool burst_step = false;
    int burst_count = 0;
    float burst_rpm = 0.0f;
    std::string fire_mode = "auto";
    if (ss->mode == ids::MODE_PLAYING && ss->player_vid) {
        auto* plm = ss->entities.get_mut(*ss->player_vid);
        if (plm && plm->equipped_gun_vid.has_value()) {
            const GunInstance* giq = ss->guns.get(*plm->equipped_gun_vid);
            const GunDef* gdq = nullptr;
            if (luam && giq) {
                for (auto const& g : luam->guns()) if (g.type == giq->def_type) { gdq = &g; break; }
            }
            if (gdq) { fire_mode = gdq->fire_mode; burst_count = gdq->burst_count; burst_rpm = gdq->burst_rpm; }
            GunInstance* gimq = ss->guns.get(*plm->equipped_gun_vid);
            if (gimq) {
                gimq->burst_timer = std::max(0.0f, gimq->burst_timer - TIMESTEP);
                if (gdq) gimq->spread_recoil_deg = std::max(0.0f, gimq->spread_recoil_deg - gdq->control * TIMESTEP);
                if (fire_mode == "auto") fire_request = trig_held;
                else if (fire_mode == "single") fire_request = trig_edge;
                else if (fire_mode == "burst") {
                    if (trig_edge && gimq->burst_remaining == 0 && burst_count > 0) gimq->burst_remaining = burst_count;
                    if (gimq->burst_remaining > 0 && gimq->burst_timer == 0.0f) { fire_request = true; burst_step = true; }
                }
            }
        } else {
            fire_request = false;
        }
    } else {
        fire_request = trig_held;
    }
    if (!(ss->mode == ids::MODE_PLAYING && ss->input_lockout_timer == 0.0f && fire_request && can_fire)) return;

    glm::vec2 p = ss->player_vid ? ss->entities.get(*ss->player_vid)->pos
                                  : glm::vec2{(float)ss->stage.get_width() / 2.0f, (float)ss->stage.get_height() / 2.0f};
    int ww = static_cast<int>(gg->window_dims.x), wh = static_cast<int>(gg->window_dims.y);
    if (gg->renderer) SDL_GetRendererOutputSize(gg->renderer, &ww, &wh);
    float inv_scale = 1.0f / (TILE_SIZE * gg->play_cam.zoom);
    glm::vec2 m = {gg->play_cam.pos.x + (static_cast<float>(ss->mouse_inputs.pos.x) - static_cast<float>(ww) * 0.5f) * inv_scale,
                   gg->play_cam.pos.y + (static_cast<float>(ss->mouse_inputs.pos.y) - static_cast<float>(wh) * 0.5f) * inv_scale};
    glm::vec2 aim = glm::normalize(m - p);
    glm::vec2 dir = aim;
    if (glm::any(glm::isnan(dir))) dir = {1.0f, 0.0f};
    float rpm = 600.0f;
    bool fired = true;
    int proj_type = 0;
    float proj_speed = 20.0f;
    glm::vec2 proj_size{0.2f, 0.2f};
    int proj_steps = 2;
    int proj_sprite_id = -1;
    int ammo_type = 0;
    if (ss->player_vid) {
        auto* plm = ss->entities.get_mut(*ss->player_vid);
        if (plm && plm->equipped_gun_vid.has_value()) {
            const GunInstance* gi = ss->guns.get(*plm->equipped_gun_vid);
            const GunDef* gd = nullptr;
            if (luam && gi) {
                for (auto const& g : luam->guns()) if (g.type == gi->def_type) { gd = &g; break; }
            }
            if (gd && gi) {
                rpm = (gd->rpm > 0.0f) ? gd->rpm : rpm;
                if (luam && gd->projectile_type != 0) {
                    if (auto const* pd = luam->find_projectile(gd->projectile_type)) {
                        proj_type = pd->type; proj_speed = pd->speed; proj_size = {pd->size_x, pd->size_y}; proj_steps = pd->physics_steps;
                        if (!pd->sprite.empty() && pd->sprite.find(':') != std::string::npos) proj_sprite_id = try_get_sprite_id(pd->sprite);
                    }
                }
                ammo_type = gi->ammo_type;
                if (luam && ammo_type != 0) {
                    if (auto const* ad = luam->find_ammo(ammo_type)) {
                        if (ad->speed > 0.0f) proj_speed = ad->speed;
                        proj_size = {ad->size_x, ad->size_y};
                        if (!ad->sprite.empty() && ad->sprite.find(':') != std::string::npos) {
                            int sid = try_get_sprite_id(ad->sprite);
                            if (sid >= 0) proj_sprite_id = sid;
                        }
                    }
                }
                GunInstance* gim = ss->guns.get(*plm->equipped_gun_vid);
                if (gim->jammed || gim->reloading || gim->reload_eject_remaining > 0.0f) {
                    fired = false;
                } else if (gim->current_mag > 0) {
                    gim->current_mag -= 1;
                } else {
                    fired = false;
                }
                if (fired) {
                    float acc = std::max(0.1f, ss->entities.get(*ss->player_vid)->stats.accuracy / 100.0f);
                    float base_dev = (gd ? gd->deviation : 0.0f) / acc;
                    float move_spread = ss->entities.get(*ss->player_vid)->move_spread_deg / acc;
                    float recoil_spread = gim->spread_recoil_deg;
                    float theta_deg = std::clamp(base_dev + move_spread + recoil_spread, MIN_SPREAD_DEG, MAX_SPREAD_DEG);
                    static thread_local std::mt19937 rng_theta{std::random_device{}()};
                    std::uniform_real_distribution<float> Uphi(-theta_deg, theta_deg);
                    float phi = Uphi(rng_theta) * 3.14159265358979323846f / 180.0f;
                    float cs = std::cos(phi), sn = std::sin(phi);
                    glm::vec2 rdir{aim.x * cs - aim.y * sn, aim.x * sn + aim.y * cs};
                    dir = glm::normalize(rdir);
                }
                if (fired) {
                    static thread_local std::mt19937 rng{std::random_device{}()};
                    std::uniform_real_distribution<float> U(0.0f, 1.0f);
                    float jc = ss->base_jam_chance + (gd ? gd->jam_chance : 0.0f);
                    jc = std::clamp(jc, 0.0f, 1.0f);
                    if (U(rng) < jc) {
                        gim->jammed = true; gim->unjam_progress = 0.0f; fired = false;
                        if (luam) { luam->call_gun_on_jam(gim->def_type, *plm); if (plm->def_type) luam->call_entity_on_gun_jam(plm->def_type, *plm); }
                        if (aa) play_sound(gd->sound_jam.empty() ? "base:ui_cant" : gd->sound_jam);
                        ss->alerts.push_back({"Gun jammed! Mash SPACE", 0.0f, 2.0f, false});
                        if (ss->player_vid) if (auto* pm = ss->metrics_for(*ss->player_vid)) pm->jams += 1;
                    }
                }
                if (fired && ss->player_vid) if (auto* pm = ss->metrics_for(*ss->player_vid)) pm->shots_fired += 1;
                if (fired && gd && gim) gim->spread_recoil_deg = std::min(gim->spread_recoil_deg + gd->recoil, gd->max_recoil_spread_deg);
            }
        }
    }
    if (fired) {
        int pellets = 1;
        if (ss->player_vid) {
            auto* plm = ss->entities.get_mut(*ss->player_vid);
            if (plm && plm->equipped_gun_vid.has_value()) {
                if (const GunInstance* gi = ss->guns.get(*plm->equipped_gun_vid)) {
                    const GunDef* gd = nullptr; if (luam) { for (auto const& g : luam->guns()) if (g.type == gi->def_type) { gd = &g; break; } }
                    if (gd && gd->pellets_per_shot > 1) pellets = gd->pellets_per_shot;
                }
            }
        }
        float theta_deg_for_shot = 0.0f;
        if (ss->player_vid) {
            auto* plm = ss->entities.get_mut(*ss->player_vid);
            if (plm && plm->equipped_gun_vid.has_value()) {
                if (const GunInstance* gi = ss->guns.get(*plm->equipped_gun_vid)) {
                    const GunDef* gd = nullptr; if (luam) { for (auto const& g : luam->guns()) if (g.type == gi->def_type) { gd = &g; break; } }
                    if (gd) {
                        float acc = std::max(0.1f, plm->stats.accuracy / 100.0f);
                        float base_dev = gd->deviation / acc;
                        float move_spread = plm->move_spread_deg / acc;
                        float recoil_spread = const_cast<GunInstance*>(gi)->spread_recoil_deg;
                        theta_deg_for_shot = std::clamp(base_dev + move_spread + recoil_spread, MIN_SPREAD_DEG, MAX_SPREAD_DEG);
                    }
                }
            }
        }
        static thread_local std::mt19937 rng_theta2{std::random_device{}()};
        std::uniform_real_distribution<float> Uphi2(-theta_deg_for_shot, theta_deg_for_shot);
        for (int i = 0; i < pellets; ++i) {
            float phi = Uphi2(rng_theta2) * 3.14159265358979323846f / 180.0f;
            float cs = std::cos(phi), sn = std::sin(phi);
            glm::vec2 pdir{aim.x * cs - aim.y * sn, aim.x * sn + aim.y * cs};
            pdir = glm::normalize(pdir);
            glm::vec2 sp = p + pdir * GUN_MUZZLE_OFFSET_UNITS;
            auto* pr = ss ? ss->projectiles.spawn(sp, pdir * proj_speed, proj_size, proj_steps, proj_type) : nullptr;
            if (pr && ss->player_vid) pr->owner = ss->player_vid;
            if (pr) {
                pr->sprite_id = proj_sprite_id;
                pr->ammo_type = ammo_type;
                float base_dmg = 1.0f;
                if (luam && ss->player_vid) {
                    if (auto* plmm = ss->entities.get_mut(*ss->player_vid)) {
                        if (plmm->equipped_gun_vid.has_value()) {
                            if (const GunInstance* gi2 = ss->guns.get(*plmm->equipped_gun_vid)) {
                                const GunDef* gd2 = nullptr; for (auto const& g : luam->guns()) if (g.type == gi2->def_type) { gd2 = &g; break; }
                                if (gd2) base_dmg = gd2->damage;
                            }
                        }
                    }
                }
                float dmg_mult = 1.0f, armor_pen = 0.0f, shield_mult = 1.0f, range_units = 0.0f;
                if (luam && ammo_type != 0) {
                    if (auto const* ad = luam->find_ammo(ammo_type)) {
                        dmg_mult = ad->damage_mult; armor_pen = ad->armor_pen; shield_mult = ad->shield_mult; range_units = ad->range_units;
                        pr->pierce_remaining = std::max(0, ad->pierce_count);
                    }
                }
                pr->base_damage = base_dmg * dmg_mult;
                pr->armor_pen = armor_pen;
                pr->shield_mult = shield_mult;
                pr->max_range_units = range_units;
            }
            (void)pr;
        }
        if (ss->player_vid) {
            auto* plm = ss->entities.get_mut(*ss->player_vid);
            if (plm && plm->equipped_gun_vid.has_value()) {
                const GunInstance* gi = ss->guns.get(*plm->equipped_gun_vid);
                const GunDef* gd = nullptr; if (luam && gi) { for (auto const& g : luam->guns()) if (g.type == gi->def_type) { gd = &g; break; } }
                if (aa) play_sound((gd && !gd->sound_fire.empty()) ? gd->sound_fire : "base:small_shoot");
            } else {
                if (aa) play_sound("base:small_shoot");
            }
        } else {
            if (aa) play_sound("base:small_shoot");
        }
        if (luam && ss->player_vid) {
            auto* plm = ss->entities.get_mut(*ss->player_vid);
            if (plm) {
                if (auto* inv = (ss->player_vid ? ss->inv_for(*ss->player_vid) : nullptr)) for (const auto& entry : inv->entries) {
                    if (entry.kind == INV_ITEM) if (const ItemInstance* inst = ss->items.get(entry.vid)) luam->call_item_on_shoot(inst->def_type, *plm);
                }
            }
        }
        if (ss->player_vid && fire_mode == "burst" && burst_step && burst_rpm > 0.0f) {
            ss->gun_cooldown = std::max(0.01f, 60.0f / burst_rpm);
            auto* plm2 = ss->entities.get_mut(*ss->player_vid);
            if (plm2 && plm2->equipped_gun_vid.has_value()) {
                if (auto* gim2 = ss->guns.get(*plm2->equipped_gun_vid)) {
                    gim2->burst_remaining = std::max(0, gim2->burst_remaining - 1);
                    gim2->burst_timer = ss->gun_cooldown;
                }
            }
        } else {
            ss->gun_cooldown = std::max(0.05f, 60.0f / rpm);
            if (ss->player_vid && fire_mode == "burst") {
                auto* plm2 = ss->entities.get_mut(*ss->player_vid);
                if (plm2 && plm2->equipped_gun_vid.has_value()) {
                    auto* gim2 = ss->guns.get(*plm2->equipped_gun_vid);
                    if (gim2 && gim2->burst_remaining == 0) gim2->burst_timer = 0.0f;
                }
            }
        }
    }
}

void update_unjam() {
    if (!ss || !ss->player_vid || ss->mode != ids::MODE_PLAYING) return;
    auto* plm = ss->entities.get_mut(*ss->player_vid);
    if (!plm || !plm->equipped_gun_vid.has_value()) return;
    GunInstance* gim = ss->guns.get(*plm->equipped_gun_vid);
    if (!gim || !gim->jammed) return;

    static bool prev_space = false;
    bool now_space = ss->playing_inputs.use_center;
    if (now_space && !prev_space) {
        ss->reticle_shake = std::max(ss->reticle_shake, 20.0f);
        gim->unjam_progress = std::min(1.0f, gim->unjam_progress + 0.2f);
        if (ss->player_vid) if (auto* pm = ss->metrics_for(*ss->player_vid)) pm->unjam_mashes += 1;
    }
    prev_space = now_space;
    if (gim->unjam_progress >= 1.0f) {
        gim->jammed = false;
        gim->unjam_progress = 0.0f;
        ss->reticle_shake = std::max(ss->reticle_shake, 10.0f);
        const GunDef* gd = nullptr;
        if (luam) {
            for (auto const& g : luam->guns()) if (g.type == gim->def_type) { gd = &g; break; }
        }
        if (gd) {
            if (gim->ammo_reserve > 0) {
                int dropped = gim->current_mag;
                if (dropped > 0)
                    ss->alerts.push_back({std::string("Dropped ") + std::to_string(dropped) + " bullets", 0.0f, 1.5f, false});
                // start active reload
                gim->current_mag = 0;
                gim->reloading = true;
                gim->reload_progress = 0.0f;
                gim->reload_eject_remaining = std::max(0.0f, gd->eject_time);
                gim->reload_total_time = std::max(0.1f, gd->reload_time);
                static thread_local std::mt19937 rng2{std::random_device{}()};
                auto clamp01b = [](float v) { return std::max(0.0f, std::min(1.0f, v)); };
                std::uniform_real_distribution<float> Upos2(-gd->ar_pos_variance, gd->ar_pos_variance);
                std::uniform_real_distribution<float> Usize2(-gd->ar_size_variance, gd->ar_size_variance);
                float size2 = std::clamp(gd->ar_size + Usize2(rng2), 0.02f, 0.9f);
                float center2 = clamp01b(gd->ar_pos + Upos2(rng2));
                float start2 = clamp01b(center2 - size2 * 0.5f);
                if (start2 + size2 > 1.0f) start2 = 1.0f - size2;
                gim->ar_window_start = start2;
                gim->ar_window_end = start2 + size2;
                gim->ar_consumed = false;
                ss->alerts.push_back({"Unjammed: Reloading...", 0.0f, 1.0f, false});
                if (aa) play_sound("base:unjam");
            } else {
                ss->alerts.push_back({"Unjammed: NO AMMO", 0.0f, 1.5f, false});
            }
        }
    }
}
