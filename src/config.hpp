#pragma once

#include <SDL2/SDL.h>
#include <optional>
#include <string>

#include "input_system.hpp"

// Loads a simple key=value .ini for input bindings. Lines starting with # are comments.
// Example:
// left=A
// right=D
// up=W
// down=S
// use_center=SPACE
// pick_up=E
// drop=Q
std::optional<InputBindings> load_input_bindings_from_ini(const std::string& path);

