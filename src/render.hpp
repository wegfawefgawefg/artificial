#pragma once

#include "graphics.hpp"
#include "state.hpp"
#include "input_system.hpp"
#include "sound.hpp"
#include "tex.hpp"

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

// Renders a full frame, including world and UI. Safe to call with null renderer
// (falls back to a short sleep to avoid busy-wait). Uses gfx.renderer.
void render_frame();
