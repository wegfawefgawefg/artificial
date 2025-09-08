#pragma once

#include <SDL2/SDL.h>
#include "state.hpp"
#include "graphics.hpp"
#include "settings.hpp"

struct InputBindings {
  SDL_Scancode left = SDL_SCANCODE_A;
  SDL_Scancode right = SDL_SCANCODE_D;
  SDL_Scancode up = SDL_SCANCODE_W;
  SDL_Scancode down = SDL_SCANCODE_S;

  SDL_Scancode use_left = SDL_SCANCODE_LEFT;
  SDL_Scancode use_right = SDL_SCANCODE_RIGHT;
  SDL_Scancode use_up = SDL_SCANCODE_UP;
  SDL_Scancode use_down = SDL_SCANCODE_DOWN;
  SDL_Scancode use_center = SDL_SCANCODE_SPACE;

  SDL_Scancode pick_up = SDL_SCANCODE_F;
  SDL_Scancode drop = SDL_SCANCODE_Q;
};

struct InputContext { float wheel_delta{0.0f}; };

void process_events(SDL_Event& ev, InputContext& ctx, State& state, bool& request_quit);
void build_inputs(const InputBindings& bind, const InputContext& ctx, State& state, Graphics& gfx, float dt);
void process_input_per_mode(State& state, Graphics& gfx);
