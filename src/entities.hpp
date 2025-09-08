#pragma once

#include <optional>
#include <vector>
#include "entity.hpp"

class Entities {
public:
  static constexpr std::size_t MAX = 1024;

  Entities();

  std::optional<VID> new_entity();
  void set_inactive(std::size_t id);
  void set_inactive_vid(VID vid);

  Entity* get_mut(VID vid);
  const Entity* get(VID vid) const;
  const Entity& by_id(std::size_t id) const { return items[id]; }

  std::vector<VID> active_vids() const;
  std::vector<Entity>& data() { return items; }

private:
  std::vector<Entity> items;
  std::vector<std::size_t> free_ids;
};

