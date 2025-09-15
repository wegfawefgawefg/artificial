// Progression helpers for exits and stage transitions.
// Responsibility: exit countdown, score review entry, and transitions between
// score review/next stage/playing.
#pragma once

// Update exit countdown based on player overlap; push alerts for start/cancel.
void update_exit_countdown();

// If countdown is active, decrement and enter score review when ready.
void start_score_review_if_ready();

// Handle confirm during score review to advance to next-stage info; cleans up ground items/guns.
void process_score_review_advance();

// Handle confirm during next-stage info to enter next area and regenerate room.
void process_next_stage_enter();
