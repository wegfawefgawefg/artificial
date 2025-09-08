#include "input_system.hpp"

#include <algorithm>

#include "types.hpp"

static bool is_down(SDL_Scancode sc)
{
  const Uint8 *ks = SDL_GetKeyboardState(nullptr);
  return ks[sc] != 0;
}

void process_events(SDL_Event &ev, InputContext &ctx, State &state, bool &request_quit)
{
  switch (ev.type)
  {
  case SDL_QUIT:
    request_quit = true;
    break;
  case SDL_MOUSEWHEEL:
    ctx.wheel_delta += static_cast<float>(ev.wheel.preciseY);
    break;
  default:
    break;
  }
  int mx = 0, my = 0;
  Uint32 mbtn = SDL_GetMouseState(&mx, &my);
  state.mouse_inputs.left = (mbtn & SDL_BUTTON(SDL_BUTTON_LEFT)) != 0;
  state.mouse_inputs.right = (mbtn & SDL_BUTTON(SDL_BUTTON_RIGHT)) != 0;
  state.mouse_inputs.pos = {mx, my};
}

void build_inputs(const InputBindings &bind, const InputContext &ctx, State &state, Graphics &gfx, float dt)
{
  if (is_down(SDL_SCANCODE_ESCAPE))
    state.running = false;
  state.mouse_inputs.scroll = ctx.wheel_delta;

  MenuInputs menu{};
  menu.left = is_down(SDL_SCANCODE_LEFT) || is_down(SDL_SCANCODE_A);
  menu.right = is_down(SDL_SCANCODE_RIGHT) || is_down(SDL_SCANCODE_D);
  menu.up = is_down(SDL_SCANCODE_UP) || is_down(SDL_SCANCODE_W);
  menu.down = is_down(SDL_SCANCODE_DOWN) || is_down(SDL_SCANCODE_S);
  menu.confirm = is_down(SDL_SCANCODE_RETURN) || is_down(SDL_SCANCODE_SPACE);
  menu.back = is_down(SDL_SCANCODE_ESCAPE) || is_down(SDL_SCANCODE_BACKSPACE);
  state.menu_input_debounce_timers.step(dt);
  state.menu_inputs = state.menu_input_debounce_timers.debounce(menu);

  PlayingInputs pi{};
  pi.left = is_down(bind.left);
  pi.right = is_down(bind.right);
  pi.up = is_down(bind.up);
  pi.down = is_down(bind.down);
  pi.inventory_prev = is_down(SDL_SCANCODE_COMMA);
  pi.inventory_next = is_down(SDL_SCANCODE_PERIOD);
  pi.mouse_pos = glm::vec2(state.mouse_inputs.pos);
  pi.mouse_down[0] = state.mouse_inputs.left;
  pi.mouse_down[1] = state.mouse_inputs.right;
  pi.use_left = is_down(bind.use_left);
  pi.use_right = is_down(bind.use_right);
  pi.use_up = is_down(bind.use_up);
  pi.use_down = is_down(bind.use_down);
  pi.use_center = is_down(bind.use_center);
  // Allow both bound key and legacy 'E' during transition
  pi.pick_up = is_down(bind.pick_up) || is_down(SDL_SCANCODE_E);
  pi.drop = is_down(bind.drop);
  pi.reload = is_down(bind.reload);
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
  state.playing_input_debounce_timers.step(dt);
  state.playing_inputs = state.playing_input_debounce_timers.debounce(pi);

  process_input_per_mode(state, gfx);

  if (ctx.wheel_delta != 0.0f)
  {
    const float ZOOM_INCREMENT = 0.25f;
    const float MIN_ZOOM = 0.5f;
    const float MAX_ZOOM = 32.0f;
    float dir = (ctx.wheel_delta > 0.0f) ? 1.0f : -1.0f;
    gfx.play_cam.zoom = std::clamp(gfx.play_cam.zoom + dir * ZOOM_INCREMENT, MIN_ZOOM, MAX_ZOOM);
  }

  // Toggle character panel with 'C'
  static bool prev_c = false;
  bool c_now = is_down(SDL_SCANCODE_C);
  if (c_now && !prev_c) { state.show_character_panel = !state.show_character_panel; }
  prev_c = c_now;

  // Toggle camera follow with 'V'
  static bool prev_v = false;
  bool v_now = is_down(SDL_SCANCODE_V);
  if (v_now && !prev_v) { state.camera_follow_enabled = !state.camera_follow_enabled; }
  prev_v = v_now;
}

void process_input_per_mode(State &state, Graphics &gfx)
{
  if (state.mode == ids::MODE_TITLE)
  {
    if (state.menu_inputs.confirm)
    {
      state.mode = ids::MODE_PLAYING;
      gfx.camera.zoom = 2.0f;
      gfx.play_cam.zoom = 2.0f;
    }
  }
  else if (state.mode == ids::MODE_GAME_OVER)
  {
    if (state.menu_inputs.confirm)
      state.mode = ids::MODE_TITLE;
  }
}
