#pragma once

#include <glm/glm.hpp>
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_image.h>
#include <string>
#include <unordered_map>
#include <vector>

#include "sprites.hpp" // for SpriteDef metadata

inline constexpr float TILE_SIZE = 16.0f;

struct Camera2D {
    glm::vec2 target{0.0f, 0.0f};
    glm::vec2 offset{0.0f, 0.0f};
    float rotation{0.0f};
    float zoom{2.0f};
};

struct PlayCam {
    glm::vec2 pos{0.0f, 0.0f};
    float zoom{2.0f};
};

struct Graphics {
    // Windowing
    SDL_Window* window{nullptr};
    SDL_Renderer* renderer{nullptr};
    TTF_Font* ui_font{nullptr};

    glm::uvec2 window_dims{1280, 720};
    glm::uvec2 dims{1280, 720};
    bool fullscreen{false};

    Camera2D camera{};
    PlayCam play_cam{};

    // Sprite registry and definitions
    std::unordered_map<std::string, int> sprite_name_to_id;
    std::vector<std::string> sprite_id_to_name; // index == id
    std::vector<SpriteDef> sprite_defs_by_id;   // index == id

    // Textures keyed by sprite id
    std::unordered_map<int, SDL_Texture*> textures_by_id;
};

// Initialize window/renderer into Graphics. When headless is true, no window/renderer is created.
// Returns true on success (including headless); false if windowed init fails.
bool init_graphics(bool headless);

// Destroy renderer/window if present and reset pointers.
void cleanup_graphics();

// Initialize UI font into gfx.ui_font by scanning fonts/ for a .ttf. Safe if already initialized.
bool init_font(const char* fonts_dir = "fonts", int pt_size = 20);

// Try initializing SDL video with a specific driver; logs on failure.
bool try_init_video_with_driver(const char* driver);

// ---- Asset Management ----

// Registry operations
void build_sprite_name_id_mapping(const std::vector<std::string>& names);
int add_or_get_sprite_id(const std::string& name);
int try_get_sprite_id(const std::string& name);

// Sprite definitions operations
void rebuild_sprite_mapping(const std::vector<SpriteDef>& defs);
const SpriteDef* get_sprite_def_by_id(int id);
const SpriteDef* try_get_sprite_def(const std::string& name);

// Textures operations
void clear_textures();
bool load_all_textures_in_sprite_lookup();
SDL_Texture* get_texture(int sprite_id);
