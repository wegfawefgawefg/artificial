#include "step.hpp"
#include "globals.hpp"
#include "settings.hpp"

#include "camera.hpp"
#include "pickups_inventory.hpp"
#include "player_combat.hpp"
#include "progression.hpp"
#include "scripting_ticks.hpp"
#include "projectiles_step.hpp"
#include "player_movement.hpp"

#include <algorithm>

// One fixed-timestep simulation tick for MODE_PLAYING.
void step_playing() {
    // Before-physics ticking (opt-in)
    pre_physics_ticks();

    // Movement + physics: player controlled + NPC wander; keep inside non-block tiles
    update_movement_and_collision();

    // Shield regen + reload progress
    update_shields_and_reload_progress();

    // Auto-pickup powerups on overlap
    auto_pickup_powerups();

    // Manual pickup + separation
    if (ss->player_vid) {
        handle_manual_pickups();
        separate_ground_items();
    }

    // Toggle drop mode and handle number row actions
    toggle_drop_mode();
    handle_inventory_hotbar();

    // Accumulate metrics and decrement lockouts
    ss->metrics.time_in_stage += TIMESTEP;
    ss->input_lockout_timer = std::max(0.0f, ss->input_lockout_timer - TIMESTEP);
    ss->pickup_lockout = std::max(0.0f, ss->pickup_lockout - TIMESTEP);

    // Exit countdown and transitions
    update_exit_countdown();
    start_score_review_if_ready();

    // Camera follow
    update_camera_follow();

    // Combat: reload edges + firing + unjam
    update_reload_active();
    update_trigger_and_fire();
    update_unjam();

    // Projectiles update
    step_projectiles_and_hits();

    // After-physics ticking (opt-in)
    post_physics_ticks();
}
