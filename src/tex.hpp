#pragma once
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <string>
#include <unordered_map>
#include "sprites.hpp"

class TextureStore {
public:
  ~TextureStore();
  bool load_all(SDL_Renderer* r, const SpriteStore& sprites);
  SDL_Texture* get(int sprite_id) const;
private:
  std::unordered_map<int, SDL_Texture*> by_id_;
};

