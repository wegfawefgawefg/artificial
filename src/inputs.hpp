#pragma once

#include <array>
#include <algorithm>
#include <glm/glm.hpp>

struct MouseInputs {
  bool left{false};
  bool right{false};
  glm::ivec2 pos{0, 0};
  float scroll{0.0f};
  static MouseInputs make() { return {}; }
};

struct MenuInputs {
  bool left{false}, right{false}, up{false}, down{false};
  bool confirm{false};
  bool back{false};
  static MenuInputs make() { return {}; }
};

struct MenuInputDebounceTimers {
  float left{0.0f}, right{0.0f}, up{0.0f}, down{0.0f};
  void step(float dt) {
    left = std::max(0.0f, left - dt);
    right = std::max(0.0f, right - dt);
    up = std::max(0.0f, up - dt);
    down = std::max(0.0f, down - dt);
  }
  MenuInputs debounce(const MenuInputs& in) const {
    MenuInputs out = in;
    out.left = (left == 0.0f) && in.left;
    out.right = (right == 0.0f) && in.right;
    out.up = (up == 0.0f) && in.up;
    out.down = (down == 0.0f) && in.down;
    return out;
  }
  static MenuInputDebounceTimers make() { return {}; }
};

struct PlayingInputs {
  bool left{false}, right{false}, up{false}, down{false};
  bool inventory_prev{false}, inventory_next{false};
  glm::vec2 mouse_pos{0.0f, 0.0f};
  std::array<bool, 2> mouse_down{false, false};
  bool num_row_1{false}, num_row_2{false}, num_row_3{false}, num_row_4{false}, num_row_5{false};
  bool num_row_6{false}, num_row_7{false}, num_row_8{false}, num_row_9{false}, num_row_0{false};
  bool select_inventory_index_0{false}, select_inventory_index_1{false}, select_inventory_index_2{false}, select_inventory_index_3{false}, select_inventory_index_4{false};
  bool select_inventory_index_5{false}, select_inventory_index_6{false}, select_inventory_index_7{false}, select_inventory_index_8{false}, select_inventory_index_9{false};
  bool use_left{false}, use_right{false}, use_up{false}, use_down{false}, use_center{false};
  bool pick_up{false}, drop{false};
  bool reload{false};
  static PlayingInputs make() { return {}; }
};

struct PlayingInputDebounceTimers {
  float inventory_prev{0.0f};
  float inventory_next{0.0f};
  void step(float dt) {
    inventory_prev = std::max(0.0f, inventory_prev - dt);
    inventory_next = std::max(0.0f, inventory_next - dt);
  }
  PlayingInputs debounce(const PlayingInputs& in) const {
    PlayingInputs out = in;
    out.inventory_prev = (inventory_prev == 0.0f) && in.inventory_prev;
    out.inventory_next = (inventory_next == 0.0f) && in.inventory_next;
    return out;
  }
  static PlayingInputDebounceTimers make() { return {}; }
};
