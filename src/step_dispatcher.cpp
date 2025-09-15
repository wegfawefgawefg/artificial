// Temporary dispatcher that forwards to the legacy step implementation.
// This scaffolds the target architecture without changing behavior yet.

#include "step.hpp"
#include "globals.hpp"
#include "settings.hpp"
#include "alerts.hpp"
#include "crates.hpp"

// Forwarder: branch to mode-specific steps as they come online.
void step() {
    if (!ss) return;
    // Age alerts once per frame
    age_and_prune_alerts(ss->dt);

    // Fixed timestep catch-up. Apply to all modes for consistent timing.
    ss->time_since_last_update += static_cast<float>(ss->dt);
    while (ss->time_since_last_update > TIMESTEP) {
        ss->time_since_last_update -= TIMESTEP;
        if (ss->frame_pause > 0) { ss->frame_pause -= 1; return; }

        switch (ss->mode) {
            case ids::MODE_PLAYING:
                step_playing();
                break;
            case ids::MODE_TITLE:
                step_title();
                break;
            case ids::MODE_SCORE_REVIEW:
                step_score_review();
                break;
            case ids::MODE_NEXT_STAGE:
                step_next_stage();
                break;
            case ids::MODE_GAME_OVER:
                step_game_over();
                break;
            default:
                // Unknown mode: do nothing this tick
                break;
        }

        ss->scene_frame = ss->scene_frame + 1u;
    }

    // Per-frame updates that should not run inside the fixed-step loop
    update_crates_open();
}
