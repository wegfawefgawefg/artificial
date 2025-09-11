#pragma once

#include "graphics.hpp"
#include "projectiles.hpp"
#include "state.hpp"

// Pre-physics ticking for guns/items (Lua-driven).
void sim_pre_physics_ticks();

// Player/NPC movement, dash, per-axis tile collision, movement spread.
void sim_move_and_collide();

// Shield regen and active reload progress/completion on equipped gun.
void sim_shield_and_reload();

// Toggle drop mode (Q edge).
void sim_toggle_drop_mode();

// Number row actions: select, use, or drop items/guns.
void sim_inventory_number_row();

// Gentle separation between overlapping ground items/guns.
void sim_ground_repulsion();

// Update crate open progress when player overlaps; handle opening and drops.
void sim_update_crates_open();

// Step projectiles and resolve hits (damage, drops, metrics).
void sim_step_projectiles();

// Manual pickup handling (F key) for best-overlap ground item/gun with sounds and metrics.
void sim_handle_pickups();

// One full frame of input + simulation (fixed-step updates inside).
// Computes dt internally, polls events, builds inputs, runs updates.
void sim_step();
