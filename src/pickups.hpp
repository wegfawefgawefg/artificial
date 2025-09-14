#pragma once

#include <cstdint>
#include <glm/glm.hpp>
#include <string>
#include <vector>

struct Pickup {
    bool active{false};
    uint32_t type{0};
    std::string name{};
    glm::vec2 pos{0.0f, 0.0f};
    int sprite_id{-1};
};

struct PickupsPool {
  public:
    static constexpr std::size_t MAX = 1024;
    PickupsPool() {
        items.resize(MAX);
    }
    Pickup* spawn(uint32_t type, const std::string& name, glm::vec2 pos) {
        for (auto& it : items)
            if (!it.active) {
                it.active = true;
                it.type = type;
                it.name = name;
                it.pos = pos;
                return &it;
            }
        return nullptr;
    }
    void clear() {
        for (auto& it : items)
            it.active = false;
    }
    std::vector<Pickup>& data() {
        return items;
    }
    const std::vector<Pickup>& data() const {
        return items;
    }

  private:
    std::vector<Pickup> items;
};
