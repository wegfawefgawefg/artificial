#pragma once

struct Settings {
    float frames_per_second{144.0f};
    float timestep{1.0f / 144.0f};
    float camera_follow_factor{0.25f};
    float exit_countdown_seconds{5.0f};
};

