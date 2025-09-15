// Camera follow implementation.
// Moves gg->play_cam.pos toward player and mouse mix when enabled.
#include "globals.hpp"
#include "graphics.hpp"
#include "state.hpp"

#include <SDL2/SDL.h>
#include <algorithm>

void update_camera_follow() {
    if (!ss || !gg) return;
    if (ss->player_vid) {
        const Entity* p = ss->entities.get(*ss->player_vid);
        if (p) {
            int ww = static_cast<int>(gg->window_dims.x), wh = static_cast<int>(gg->window_dims.y);
            if (gg->renderer) SDL_GetRendererOutputSize(gg->renderer, &ww, &wh);
            float zx = gg->play_cam.zoom;
            float sx = static_cast<float>(ss->mouse_inputs.pos.x);
            float sy = static_cast<float>(ss->mouse_inputs.pos.y);
            // screen pixels to world units
            glm::vec2 mouse_world = p->pos; // default fallback
            float inv_scale = 1.0f / (TILE_SIZE * zx);
            mouse_world.x = gg->play_cam.pos.x + (sx - static_cast<float>(ww) * 0.5f) * inv_scale;
            mouse_world.y = gg->play_cam.pos.y + (sy - static_cast<float>(wh) * 0.5f) * inv_scale;

            glm::vec2 target = p->pos;
            if (ss->camera_follow_enabled) {
                float cff = ss->settings.camera_follow_factor;
                target = p->pos + (mouse_world - p->pos) * cff;
            }
            gg->play_cam.pos = target;
        }
    }
}
