#pragma once

#include <glm/glm.hpp>

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
    glm::uvec2 window_dims{1280, 720};
    glm::uvec2 dims{1280, 720};
    bool fullscreen{false};

    Camera2D camera{};
    PlayCam play_cam{};
};
