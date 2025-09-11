#include "tex.hpp"
#include "globals.hpp"
#include "graphics.hpp"

#include <cstdio>

TextureStore::~TextureStore() {
    for (auto& kv : by_id_)
        if (kv.second)
            SDL_DestroyTexture(kv.second);
}

bool TextureStore::load_all() {
    if (!g_gfx || !g_gfx->renderer || !g_sprite_store)
        return false;
    SDL_Renderer* r = g_gfx->renderer;
    const SpriteStore& sprites = *g_sprite_store;
    for (int id = 0; id < sprites.size(); ++id) {
        auto* def = sprites.get_def_by_id(id);
        if (!def)
            continue;
        if (def->image_path.empty())
            continue;
        SDL_Texture* tex = IMG_LoadTexture(r, def->image_path.c_str());
        if (!tex) {
            std::fprintf(stderr, "IMG_LoadTexture failed for %s: %s\n", def->image_path.c_str(),
                         IMG_GetError());
            continue;
        }
        by_id_[id] = tex;
    }
    return true;
}

SDL_Texture* TextureStore::get(int sprite_id) const {
    auto it = by_id_.find(sprite_id);
    return (it == by_id_.end()) ? nullptr : it->second;
}
