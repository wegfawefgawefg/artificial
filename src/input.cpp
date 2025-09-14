#include "globals.hpp"
#include "types.hpp"
#include "input_defs.hpp"
#include "input.hpp"

#include <algorithm>

// Forward declaration used within this translation unit
void collect_menu_inputs();

static bool is_down(SDL_Scancode sc) {
    const Uint8* ks = SDL_GetKeyboardState(nullptr);
    return ks[sc] != 0;
}

void process_event(SDL_Event& ev) {
    switch (ev.type) {
        case SDL_QUIT:
            ss->running = false;
            break;
        case SDL_MOUSEWHEEL:
            ss->input_state.wheel_delta += static_cast<float>(ev.wheel.preciseY);
            break;
        default:
            break;
    }
    int mx = 0, my = 0;
    Uint32 mbtn = SDL_GetMouseState(&mx, &my);
    ss->mouse_inputs.left = (mbtn & SDL_BUTTON(SDL_BUTTON_LEFT)) != 0;
    ss->mouse_inputs.right = (mbtn & SDL_BUTTON(SDL_BUTTON_RIGHT)) != 0;
    ss->mouse_inputs.pos = {mx, my};
}

void collect_inputs() {
    // Transfer wheel delta to one-frame mouse scroll and reset the accumulator
    ss->mouse_inputs.scroll = ss->input_state.wheel_delta;
    ss->input_state.wheel_delta = 0.0f;
    collect_menu_inputs();
}

void collect_menu_inputs() {
    MenuInputs menu{};
    menu.left = is_down(SDL_SCANCODE_LEFT) || is_down(SDL_SCANCODE_A);
    menu.right = is_down(SDL_SCANCODE_RIGHT) || is_down(SDL_SCANCODE_D);
    menu.up = is_down(SDL_SCANCODE_UP) || is_down(SDL_SCANCODE_W);
    menu.down = is_down(SDL_SCANCODE_DOWN) || is_down(SDL_SCANCODE_S);
    menu.confirm = is_down(SDL_SCANCODE_RETURN) || is_down(SDL_SCANCODE_SPACE);
    menu.back = is_down(SDL_SCANCODE_ESCAPE) || is_down(SDL_SCANCODE_BACKSPACE);
    ss->menu_input_debounce_timers.step(static_cast<float>(ss->dt));
    ss->menu_inputs = ss->menu_input_debounce_timers.debounce(menu);
}

void process_inputs() {
    if (is_down(SDL_SCANCODE_ESCAPE))
        ss->running = false;

    if (ss->mode == ids::MODE_TITLE) {
        process_inputs_title();
    } else if (ss->mode == ids::MODE_PLAYING) {
        process_inputs_playing();
    } else if (ss->mode == ids::MODE_GAME_OVER) {
        if (ss->menu_inputs.confirm)
            ss->mode = ids::MODE_TITLE;
    }
}



void process_inputs_title() {
    if (ss->menu_inputs.confirm) {
        ss->mode = ids::MODE_PLAYING;
        if (gg) {
            gg->camera.zoom = 2.0f;
            gg->play_cam.zoom = 2.0f;
        }
    }
}

void process_inputs_playing() {
    PlayingInputs pi{};
    const InputBindings& b = ss->input_binds;
    pi.left = is_down(b.left);
    pi.right = is_down(b.right);
    pi.up = is_down(b.up);
    pi.down = is_down(b.down);
    pi.inventory_prev = is_down(SDL_SCANCODE_COMMA);
    pi.inventory_next = is_down(SDL_SCANCODE_PERIOD);
    pi.mouse_pos = glm::vec2(ss->mouse_inputs.pos);
    pi.mouse_down[0] = ss->mouse_inputs.left;
    pi.mouse_down[1] = ss->mouse_inputs.right;
    pi.use_left = is_down(b.use_left);
    pi.use_right = is_down(b.use_right);
    pi.use_up = is_down(b.use_up);
    pi.use_down = is_down(b.use_down);
    pi.use_center = is_down(b.use_center);
    // Manual pickup key
    pi.pick_up = is_down(b.pick_up);
    pi.drop = is_down(b.drop);
    pi.reload = is_down(b.reload);
    pi.dash = is_down(b.dash);
    // number row keys 1..0
    pi.num_row_1 = is_down(SDL_SCANCODE_1);
    pi.num_row_2 = is_down(SDL_SCANCODE_2);
    pi.num_row_3 = is_down(SDL_SCANCODE_3);
    pi.num_row_4 = is_down(SDL_SCANCODE_4);
    pi.num_row_5 = is_down(SDL_SCANCODE_5);
    pi.num_row_6 = is_down(SDL_SCANCODE_6);
    pi.num_row_7 = is_down(SDL_SCANCODE_7);
    pi.num_row_8 = is_down(SDL_SCANCODE_8);
    pi.num_row_9 = is_down(SDL_SCANCODE_9);
    pi.num_row_0 = is_down(SDL_SCANCODE_0);

    // Zoom with mouse wheel (use per-frame scroll delta)
    if (ss->mouse_inputs.scroll != 0.0f) {
        const float ZOOM_INCREMENT = 0.25f;
        const float MIN_ZOOM = 0.5f;
        const float MAX_ZOOM = 32.0f;
        float dir = (ss->mouse_inputs.scroll > 0.0f) ? 1.0f : -1.0f;
        if (gg) {
            gg->play_cam.zoom =
                std::clamp(gg->play_cam.zoom + dir * ZOOM_INCREMENT, MIN_ZOOM, MAX_ZOOM);
        }
    }

    static KeyEdge c;
    c.toggle(is_down(SDL_SCANCODE_C), ss->show_character_panel);

    static KeyEdge v;
    v.toggle(is_down(SDL_SCANCODE_V), ss->show_gun_panel);

    ss->playing_input_debounce_timers.step();
    ss->playing_inputs = ss->playing_input_debounce_timers.debounce(pi);
}
