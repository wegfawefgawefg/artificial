#pragma once

#include <SDL2/SDL.h>
#include <optional>
#include <string>

// Loads a simple key=value .ini for input bindings. Lines starting with # are comments.
bool load_input_bindings_from_ini(const std::string& path);
