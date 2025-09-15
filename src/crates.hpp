// Crate pool and progression.
// Responsibility: simple crate entity storage and shared-sized defaults.
#pragma once

#include <glm/glm.hpp>
#include <vector>
#include <cstddef>

// Simple crate entity pooled in State
struct Crate {
    bool active{false};
    bool opened{false};
    int def_type{0};
    glm::vec2 pos{0.0f, 0.0f};
    // ~two players wide, one tall (world units)
    glm::vec2 size{2.0f, 1.0f};
    float open_progress{0.0f};
};

struct CratesPool {
  public:
    static constexpr std::size_t MAX = 512;
    CratesPool() { items.resize(MAX); }
    Crate* spawn(glm::vec2 p, int type) {
        for (auto& c : items) {
            if (!c.active) {
                c.active = true;
                c.opened = false;
                c.def_type = type;
                c.pos = p;
                c.size = {0.5f, 0.2f};
                c.open_progress = 0.0f;
                return &c;
            }
        }
        return nullptr;
    }
    void clear() { for (auto& c : items) c.active = false; }
    std::vector<Crate>& data() { return items; }
    const std::vector<Crate>& data() const { return items; }

  private:
    std::vector<Crate> items;
};

// Wrapper for the crate progression update
void update_crates_open();
