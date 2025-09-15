// Lua-driven ticking for guns/items/entities.
// Responsibility: run registered Lua hooks on a fixed cadence in pre-/post-
// physics phases.
#pragma once

// Pre-physics phase.
void pre_physics_ticks();

// Post-physics: guns/items with after-phase on_step/on_tick and entity after-phase.
void post_physics_ticks();
