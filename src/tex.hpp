#pragma once
#include "sprites.hpp"

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <string>
#include <unordered_map>

class TextureStore {
  public:
    ~TextureStore();
    // Loads textures using globals: g_gfx->renderer and *g_sprite_store
    bool load_all();
    SDL_Texture* get(int sprite_id) const;

  private:

  
    std::unordered_map<int, SDL_Texture*> by_id_;
};
