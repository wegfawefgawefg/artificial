#pragma once

#include <glm/glm.hpp>
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

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
};

// Initialize window/renderer into Graphics. When headless is true, no window/renderer is created.
// Returns true on success (including headless); false if windowed init fails.
bool init_graphics(Graphics& gfx, bool headless, const char* title, int width, int height);

// Destroy renderer/window if present and reset pointers.
void shutdown_graphics(Graphics& gfx);

// Initialize UI font into gfx.ui_font by scanning fonts/ for a .ttf. Safe if already initialized.
bool init_font(Graphics& gfx, const char* fonts_dir = "fonts", int pt_size = 20);

// Try initializing SDL video with a specific driver; logs on failure.
bool try_init_video_with_driver(const char* driver);
