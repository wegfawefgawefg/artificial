#pragma once

#include "graphics.hpp"
#include "mods.hpp"
#include "sound.hpp"
#include "sprites.hpp"
#include "state.hpp"
#include "tex.hpp"

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

struct App {
    SDL_Window* window{nullptr};
    SDL_Renderer* renderer{nullptr};
    TTF_Font* ui_font{nullptr};
    Graphics gfx{};
    SoundStore sounds{};
    ModsManager mods{"mods"};
    SpriteIdRegistry sprite_ids{};
    SpriteStore sprite_store{};
    TextureStore textures{};
};

// Try initializing SDL video with a specific driver; logs on failure.
bool try_init_video_with_driver(const char* driver);

