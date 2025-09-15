// Exit countdown and stage transition implementation.
#include "globals.hpp"
#include "settings.hpp"
#include "room.hpp"
#include "state.hpp"

#include <algorithm>
#include <glm/glm.hpp>
#include <cstdio>
#include <string>

void update_exit_countdown() {
    if (!ss || ss->mode != ids::MODE_PLAYING) return;
    const Entity* p = (ss->player_vid ? ss->entities.get(*ss->player_vid) : nullptr);
    if (p) {
        glm::vec2 half = p->half_size();
        float left = p->pos.x - half.x;
        float right = p->pos.x + half.x;
        float top = p->pos.y - half.y;
        float bottom = p->pos.y + half.y;
        float exl = static_cast<float>(ss->exit_tile.x);
        float exr = exl + 1.0f;
        float ext = static_cast<float>(ss->exit_tile.y);
        float exb = ext + 1.0f;
        bool overlaps = !(right <= exl || left >= exr || bottom <= ext || top >= exb);
        if (overlaps) {
            if (ss->exit_countdown < 0.0f) {
                ss->exit_countdown = ss->settings.exit_countdown_seconds;
                ss->alerts.push_back({"Exit reached: hold to leave", 0.0f, 2.0f, false});
                std::printf("[room] Exit reached, starting %.1fs countdown...\n", (double)(ss->settings.exit_countdown_seconds));
            }
        } else {
            if (ss->exit_countdown >= 0.0f) {
                ss->exit_countdown = -1.0f;
                ss->alerts.push_back({"Exit canceled", 0.0f, 1.5f, false});
                std::printf("[room] Exit countdown canceled (left tile).\n");
            }
        }
    }
}

void start_score_review_if_ready() {
    if (!ss || ss->mode != ids::MODE_PLAYING) return;
    if (ss->exit_countdown >= 0.0f) {
        ss->exit_countdown -= TIMESTEP;
        if (ss->exit_countdown <= 0.0f) {
            ss->exit_countdown = -1.0f;
            ss->mode = ids::MODE_SCORE_REVIEW;
            ss->score_ready_timer = SCORE_REVIEW_INPUT_DELAY;
            ss->alerts.push_back({"Area complete", 0.0f, 2.5f, false});
            std::printf("[room] Countdown complete. Entering score review.\n");
            // Prepare review stats and animation
            ss->review_stats.clear();
            ss->review_revealed = 0;
            ss->review_next_stat_timer = 0.0f;
            ss->review_number_tick_timer = 0.0f;
            auto add_header = [&](const std::string& s) {
                ss->review_stats.push_back(State::ReviewStat{s, 0.0, 0.0, true, true});
            };
            auto add_stat = [&](const std::string& s, double v) {
                ss->review_stats.push_back(State::ReviewStat{s, v, 0.0, false, false});
            };
            std::uint64_t total_shots_fired = 0, total_shots_hit = 0;
            std::uint64_t total_enemies_slain = ss->metrics.enemies_slain;
            std::uint64_t total_powerups_picked = 0, total_items_picked = 0, total_guns_picked = 0, total_items_dropped = 0, total_guns_dropped = 0, total_damage_dealt = 0;
            for (auto const& e : ss->entities.data()) {
                if (!e.active || e.type_ != ids::ET_PLAYER) continue;
                const auto* pm = ss->metrics_for(e.vid); if (!pm) continue;
                total_shots_fired += pm->shots_fired; total_shots_hit += pm->shots_hit;
                total_powerups_picked += pm->powerups_picked; total_items_picked += pm->items_picked; total_guns_picked += pm->guns_picked; total_items_dropped += pm->items_dropped; total_guns_dropped += pm->guns_dropped; total_damage_dealt += pm->damage_dealt;
            }
            std::int64_t missed_powerups = (std::int64_t)ss->metrics.powerups_spawned - (std::int64_t)total_powerups_picked;
            std::int64_t missed_items = (std::int64_t)ss->metrics.items_spawned - (std::int64_t)total_items_picked;
            std::int64_t missed_guns = (std::int64_t)ss->metrics.guns_spawned - (std::int64_t)total_guns_picked;
            if (missed_powerups < 0) missed_powerups = 0;
            if (missed_items < 0) missed_items = 0;
            if (missed_guns < 0) missed_guns = 0;
            add_stat("Time (s)", ss->metrics.time_in_stage);
            add_stat("Crates opened", (double)ss->metrics.crates_opened);
            add_stat("Enemies slain", (double)total_enemies_slain);
            add_stat("Damage dealt", (double)total_damage_dealt);
            add_stat("Shots fired (total)", (double)total_shots_fired);
            add_stat("Shots hit (total)", (double)total_shots_hit);
            double acc_total = total_shots_fired ? (100.0 * (double)total_shots_hit / (double)total_shots_fired) : 0.0;
            add_stat("Accuracy total (%)", acc_total);
            add_stat("Powerups picked (total)", (double)total_powerups_picked);
            add_stat("Items picked (total)", (double)total_items_picked);
            add_stat("Guns picked (total)", (double)total_guns_picked);
            add_stat("Items dropped (total)", (double)total_items_dropped);
            add_stat("Guns dropped (total)", (double)total_guns_dropped);
            add_stat("Missed powerups", (double)missed_powerups);
            add_stat("Missed items", (double)missed_items);
            add_stat("Missed guns", (double)missed_guns);
            int pidx = 1;
            for (auto const& e : ss->entities.data()) {
                if (!e.active || e.type_ != ids::ET_PLAYER) continue;
                const auto* pm = ss->metrics_for(e.vid); if (!pm) continue;
                char hdr[32]; std::snprintf(hdr, sizeof(hdr), "Player %d", pidx++);
                add_header(hdr);
                add_stat("  Shots fired", (double)pm->shots_fired);
                add_stat("  Shots hit", (double)pm->shots_hit);
                double acc = pm->shots_fired ? (100.0 * (double)pm->shots_hit / (double)pm->shots_fired) : 0.0;
                add_stat("  Accuracy (%)", acc);
                add_stat("  Enemies slain", (double)pm->enemies_slain);
                add_stat("  Dashes used", (double)pm->dashes_used);
                add_stat("  Dash distance", (double)pm->dash_distance);
                add_stat("  Powerups picked", (double)pm->powerups_picked);
                add_stat("  Items picked", (double)pm->items_picked);
                add_stat("  Guns picked", (double)pm->guns_picked);
                add_stat("  Items dropped", (double)pm->items_dropped);
                add_stat("  Guns dropped", (double)pm->guns_dropped);
                add_stat("  Reloads", (double)pm->reloads);
                add_stat("  AR success", (double)pm->active_reload_success);
                add_stat("  AR failed", (double)pm->active_reload_fail);
                add_stat("  Jams", (double)pm->jams);
                add_stat("  Unjam mashes", (double)pm->unjam_mashes);
                add_stat("  Damage dealt", (double)pm->damage_dealt);
                add_stat("  Damage taken HP", (double)pm->damage_taken_hp);
                add_stat("  Damage to shields", (double)pm->damage_taken_shield);
                add_stat("  Plates gained", (double)pm->plates_gained);
                add_stat("  Plates consumed", (double)pm->plates_consumed);
            }
        }
    }
}

static void cleanup_ground_instances() {
    for (auto& gi : ss->ground_items.data()) if (gi.active) { ss->items.free(gi.item_vid); gi.active = false; }
    for (auto& ggun : ss->ground_guns.data()) if (ggun.active) { ss->guns.free(ggun.gun_vid); ggun.active = false; }
}

void process_score_review_advance() {
    if (!ss || ss->mode != ids::MODE_SCORE_REVIEW) return;
    if (ss->score_ready_timer > 0.0f) return;
    if (ss->menu_inputs.confirm || ss->playing_inputs.use_center || ss->mouse_inputs.left) {
        cleanup_ground_instances();
        std::printf("[room] Proceeding to next area info screen.\n");
        ss->mode = ids::MODE_NEXT_STAGE;
        ss->score_ready_timer = 0.5f;
        ss->input_lockout_timer = 0.2f;
    }
}

void process_next_stage_enter() {
    if (!ss || ss->mode != ids::MODE_NEXT_STAGE) return;
    if (ss->score_ready_timer > 0.0f) return;
    if (ss->menu_inputs.confirm || ss->playing_inputs.use_center || ss->mouse_inputs.left) {
        std::printf("[room] Entering next area.\n");
        ss->alerts.push_back({"Entering next area", 0.0f, 2.0f, false});
        ss->mode = ids::MODE_PLAYING;
        generate_room();
        ss->input_lockout_timer = 0.25f;
    }
}
