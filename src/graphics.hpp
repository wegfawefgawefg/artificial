#pragma once

#include <glm/glm.hpp>
#include <SDL2/SDL.h>

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
