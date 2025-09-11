#pragma once

#include "graphics.hpp"
#include "settings.hpp"
#include "state.hpp"

#include <SDL2/SDL.h>

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
    SDL_Scancode reload = SDL_SCANCODE_R;
    SDL_Scancode dash = SDL_SCANCODE_LSHIFT;
};

struct InputContext {
    float wheel_delta{0.0f};
};

void process_events(SDL_Event& ev, bool& request_quit);
// Reads from globals: g_binds, g_input, g_state, g_gfx
void build_inputs();
void process_input_per_mode();
