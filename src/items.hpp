#pragma once

#include <vector>
#include <cstdint>
#include <string>
#include <glm/glm.hpp>

struct Item {
  bool active{false};
  std::uint32_t type{0};
  // category: 0=other, 1=usable, 2=gun
  int category{0};
  // inventory/display
  std::string name{};
  uint32_t count{1};
  uint32_t max_count{1};
  // usage
  float range{0.0f};
  float min_range{0.0f};
  float use_cooldown{0.0f};
  float use_cooldown_countdown{0.0f};
  bool consume_on_use{false};
  bool droppable{true};
  // sprite id placeholder
  int sprite_id{-1};

  bool is_stackable() const { return max_count > 1; }
};

class ItemsPool {
public:
  static constexpr std::size_t MAX = 1024;
  ItemsPool() { items.resize(MAX); }
  Item* spawn(std::uint32_t type) {
    for (auto& it : items) if (!it.active) { it.active = true; it.type = type; return &it; }
    return nullptr;
  }
  void clear() { for (auto& it : items) it.active = false; }
  std::vector<Item>& data() { return items; }
  const std::vector<Item>& data() const { return items; }
private:
  std::vector<Item> items;
};

// Ground item: a world object containing an Item at a position (requires pressing F to pick up)
struct GroundItem {
  bool active{false};
  Item item{};
  glm::vec2 pos{0.0f, 0.0f};
  glm::vec2 size{0.25f, 0.25f};
};

class GroundItemsPool {
public:
  static constexpr std::size_t MAX = 256;
  GroundItemsPool() { items.resize(MAX); }
  GroundItem* spawn(const Item& it, glm::vec2 pos) {
    for (auto& gi : items) if (!gi.active) { gi.active = true; gi.item = it; gi.pos = pos; return &gi; }
    return nullptr;
  }
  void clear() { for (auto& gi : items) gi.active = false; }
  std::vector<GroundItem>& data() { return items; }
  const std::vector<GroundItem>& data() const { return items; }
private:
  std::vector<GroundItem> items;
};
