#pragma once

#include "graphics.hpp"
#include "state.hpp"
#include "projectiles.hpp"
#include "input_system.hpp"
#include "sound.hpp"
#include "tex.hpp"

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

// Renders a full frame, including world and UI. Safe to call with null renderer
// (falls back to a short sleep to avoid busy-wait).
void render_frame(SDL_Window* window,
                  SDL_Renderer* renderer,
                  const TextureStore& textures,
                  TTF_Font* ui_font,
                  State& state,
                  Graphics& gfx,
                  double dt_sec,
                  const InputBindings& binds,
                  const Projectiles& projectiles,
                  SoundStore& sounds);
