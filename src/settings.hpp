#pragma once

inline constexpr float INVENTORY_SELECTION_DEBOUNCE_INTERVAL = 0.2f;
inline constexpr int MAX_ENTITIES = 1024;
inline constexpr int MAX_PROJECTILES = 1024;
inline constexpr float FRAMES_PER_SECOND = 144.0f;
inline constexpr float TIMESTEP = 1.0f / FRAMES_PER_SECOND;
inline constexpr float HOT_RELOAD_POLL_INTERVAL = 0.5f;   // seconds
inline constexpr float CAMERA_FOLLOW_FACTOR = 0.25f;      // fraction towards cursor when enabled
inline constexpr float PLAYER_SPEED_UNITS_PER_SEC = 2.5f; // slowed base speed
// Dash parameters
inline constexpr float DASH_SPEED_UNITS_PER_SEC = 9.0f;
inline constexpr float DASH_TIME_SECONDS = 0.15f;
inline constexpr float DASH_COOLDOWN_SECONDS = 0.8f;
// Gun rendering / muzzle offset (world units from player center)
inline constexpr float GUN_HOLD_OFFSET_UNITS = 0.30f;
inline constexpr float GUN_MUZZLE_OFFSET_UNITS = 0.40f; // where bullets spawn (reduced)
inline constexpr float EXIT_COUNTDOWN_SECONDS = 5.0f;
inline constexpr float SCORE_REVIEW_INPUT_DELAY = 0.8f;
// Pickup tuning
inline constexpr float PICKUP_DEBOUNCE_SECONDS = 0.20f; // minimum time between manual pickups
// Accuracy tuning
inline constexpr float MOVE_SPREAD_DEG_AT_BASE_SPEED = 2.0f; // deg at 350 units/s
inline constexpr float MIN_SPREAD_DEG = 0.1f;
inline constexpr float MAX_SPREAD_DEG = 20.0f;
// Movement spread dynamics (deg/sec)
inline constexpr float MOVE_SPREAD_INCREASE_DEG_PER_SEC_AT_BASE_SPEED = 8.0f; // per 350 u/s
inline constexpr float MOVE_SPREAD_DECAY_DEG_PER_SEC = 10.0f;                  // when stationary
