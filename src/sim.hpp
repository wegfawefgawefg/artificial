#pragma once

#include "graphics.hpp"
#include "projectiles.hpp"
#include "state.hpp"

// Pre-physics ticking for guns/items (Lua-driven).
void sim_pre_physics_ticks(State& state);

// Player/NPC movement, dash, per-axis tile collision, movement spread.
void sim_move_and_collide(State& state, Graphics& gfx);

// Shield regen and active reload progress/completion on equipped gun.
void sim_shield_and_reload(State& state);

// Toggle drop mode (Q edge).
void sim_toggle_drop_mode(State& state);

// Number row actions: select, use, or drop items/guns.
void sim_inventory_number_row(State& state);

// Gentle separation between overlapping ground items/guns.
void sim_ground_repulsion(State& state);

// Update crate open progress when player overlaps; handle opening and drops.
void sim_update_crates_open(State& state);

// Step projectiles and resolve hits (damage, drops, metrics).
void sim_step_projectiles(State& state, Projectiles& projectiles);

// Manual pickup handling (F key) for best-overlap ground item/gun with sounds and metrics.
struct SoundStore;
void sim_handle_pickups(State& state, SoundStore& sounds);
