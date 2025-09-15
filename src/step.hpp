#pragma once

void step();

// Mode-specific step functions (target architecture)
void step_title();
void step_playing();
void step_score_review();
void step_next_stage();
void step_game_over();
