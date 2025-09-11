#pragma once

#include "graphics.hpp"
#include "projectiles.hpp"
#include "state.hpp"

struct SDL_Renderer;

// Generate a new room/world and spawn entities.
void generate_room(State& state, Projectiles& projectiles, SDL_Renderer* renderer, Graphics& gfx);

// Helpers used during generation and spawn safety.
bool tile_blocks_entity(const State& state, int x, int y);
glm::ivec2 nearest_walkable_tile(const State& state, glm::ivec2 t, int max_radius = 8);
glm::vec2 ensure_not_in_block(const State& state, glm::vec2 pos);

