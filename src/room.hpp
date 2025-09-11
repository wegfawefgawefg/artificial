#pragma once

#include "graphics.hpp"
#include "state.hpp"

// Generate a new room/world and spawn entities.
void generate_room();

// Helpers used during generation and spawn safety.
bool tile_blocks_entity(int x, int y);
glm::ivec2 nearest_walkable_tile(glm::ivec2 t, int max_radius = 8);
glm::vec2 ensure_not_in_block(glm::vec2 pos);
