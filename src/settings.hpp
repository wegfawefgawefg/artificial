#pragma once

inline constexpr float INVENTORY_SELECTION_DEBOUNCE_INTERVAL = 0.2f;
inline constexpr int MAX_ENTITIES = 1024;
inline constexpr int MAX_PROJECTILES = 1024;
inline constexpr float FRAMES_PER_SECOND = 144.0f;
inline constexpr float TIMESTEP = 1.0f / FRAMES_PER_SECOND;
inline constexpr float HOT_RELOAD_POLL_INTERVAL = 0.5f; // seconds
inline constexpr float CAMERA_FOLLOW_FACTOR = 0.25f; // fraction towards cursor when enabled
inline constexpr float PLAYER_SPEED_UNITS_PER_SEC = 4.0f;
inline constexpr float EXIT_COUNTDOWN_SECONDS = 10.0f;
inline constexpr float SCORE_REVIEW_INPUT_DELAY = 3.0f;
