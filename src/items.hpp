#pragma once

#include "luamgr.hpp"
#include "pool.hpp"
#include "types.hpp"

#include <cstdint>
#include <glm/glm.hpp>
#include <string>
#include <vector>

struct ItemInstance {
    bool active{false};
    int def_type{0};
    uint32_t count{1};
    // Runtime state
    float use_cooldown{0.0f};
    float use_cooldown_countdown{0.0f};
    std::uint32_t modifiers_hash{0}; // items with different mods must not stack
};

class ItemsPool : public Pool<ItemInstance, 1024> {
  public:
    std::optional<VID> spawn_from_def(const ItemDef& d, uint32_t count = 1) {
        auto v = alloc();
        if (!v)
            return std::nullopt;
        if (auto* it = get(*v)) {
            it->def_type = d.type;
            it->count = count;
        }
        return v;
    }
};

// Ground item: a world object referencing an ItemInstance by VID
struct GroundItem {
    bool active{false};
    VID item_vid{};
    glm::vec2 pos{0.0f, 0.0f};
    glm::vec2 size{0.25f, 0.25f};
};

class GroundItemsPool {
  public:
    static constexpr std::size_t MAX = 1024;
    GroundItemsPool() {
        items.resize(MAX);
    }
    GroundItem* spawn(VID item_vid, glm::vec2 pos) {
        for (auto& gi : items)
            if (!gi.active) {
                gi.active = true;
                gi.item_vid = item_vid;
                gi.pos = pos;
                return &gi;
            }
        return nullptr;
    }
    void clear() {
        for (auto& gi : items)
            gi.active = false;
    }
    std::vector<GroundItem>& data() {
        return items;
    }
    const std::vector<GroundItem>& data() const {
        return items;
    }

  private:
    std::vector<GroundItem> items;
};
