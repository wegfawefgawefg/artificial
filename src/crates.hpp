#pragma once

#include <glm/glm.hpp>
#include <vector>

struct Crate {
    bool active{false};
    bool opened{false};
    glm::vec2 pos{0.0f, 0.0f};
    glm::vec2 size{0.5f, 0.5f};
    float open_progress{0.0f}; // seconds
    int def_type{0};
};

class CratesPool {
  public:
    static constexpr std::size_t MAX = 256;
    CratesPool() {
        items.resize(MAX);
    }
    Crate* spawn(glm::vec2 pos, int def_type) {
        for (auto& c : items)
            if (!c.active) {
                c.active = true;
                c.opened = false;
                c.pos = pos;
                c.open_progress = 0.0f;
                c.def_type = def_type;
                return &c;
            }
        return nullptr;
    }
    void clear() {
        for (auto& c : items)
            c.active = false;
    }
    std::vector<Crate>& data() {
        return items;
    }
    const std::vector<Crate>& data() const {
        return items;
    }

  private:
    std::vector<Crate> items;
};
