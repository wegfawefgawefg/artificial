#include "step.hpp"
#include "globals.hpp"
#include "settings.hpp"
#include "progression.hpp"
#include <algorithm>

void step_score_review() {
    if (!ss) return;
    if (ss->score_ready_timer > 0.0f)
        ss->score_ready_timer = std::max(0.0f, ss->score_ready_timer - TIMESTEP);
    process_score_review_advance();
}
