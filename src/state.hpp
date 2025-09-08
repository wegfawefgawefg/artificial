#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include <glm/glm.hpp>
#include <string>
#include <vector>

#include "types.hpp"
#include "inputs.hpp"
#include "entities.hpp"
#include "particles.hpp"
#include "stage.hpp"
#include "inventory.hpp"
#include "items.hpp"
#include "pickups.hpp"
#include "guns.hpp"

struct State {
  int mode{ids::MODE_TITLE};
  bool mouse_mode{true};
  MouseInputs mouse_inputs = MouseInputs::make();
  MenuInputs menu_inputs = MenuInputs::make();
  MenuInputDebounceTimers menu_input_debounce_timers = MenuInputDebounceTimers::make();
  PlayingInputs playing_inputs = PlayingInputs::make();
  PlayingInputDebounceTimers playing_input_debounce_timers = PlayingInputDebounceTimers::make();

  bool running{true};
  double now{0.0};
  float time_since_last_update{0.0f};
  std::uint32_t scene_frame{0};
  std::uint32_t frame{0};

  bool game_over{false};
  bool pause{false};
  bool win{false};

  std::uint32_t points{0};
  std::uint32_t deaths{0};
  std::uint32_t frame_pause{0};

  Entities entities{};
  std::optional<VID> player_vid{};
  Particles particles{};
  Stage stage{64, 36};
  Inventory inventory = Inventory::make();
  PickupsPool pickups{};
  GroundItemsPool ground_items{};
  GunsPool guns{};
  GroundGunsPool ground_guns{};

  // Firing cooldown (seconds)
  float gun_cooldown{0.0f};

  bool rebuild_render_texture{true};
  float cloud_density{0.5f};

  bool camera_follow_enabled{true};

  // Room / flow
  glm::ivec2 start_tile{-1, -1};
  glm::ivec2 exit_tile{-1, -1};
  float exit_countdown{-1.0f}; // <0 inactive, else seconds remaining
  float score_ready_timer{0.0f}; // seconds until input allowed on score screen

  // Alerts/messages (debug or UX). Rendered top-left. Age and purge.
  struct Alert { std::string text; float age{0.0f}; float ttl{2.0f}; bool purge_eof{false}; };
  std::vector<Alert> alerts;

  // Character panel (left) and gun panel (right)
  bool show_character_panel{false};
  float character_panel_slide{0.0f}; // 0..1
};
