#include "entities.hpp"

Entities::Entities() {
    items.reserve(MAX);
    free_ids.reserve(MAX);
    for (std::size_t i = 0; i < MAX; ++i) {
        Entity e{};
        e.vid.id = i;
        items.push_back(e);
        free_ids.insert(free_ids.begin(), i);
    }
}

std::optional<VID> Entities::new_entity() {
    if (free_ids.empty())
        return std::nullopt;
    std::size_t id = free_ids.back();
    free_ids.pop_back();
    Entity& e = items[id];
    e.active = true;
    e.vid.version += 1;
    return e.vid;
}

void Entities::set_inactive(std::size_t id) {
    items[id].active = false;
    free_ids.insert(free_ids.begin(), id);
}

void Entities::set_inactive_vid(VID vid) {
    Entity& e = items[vid.id];
    if (e.active && e.vid.version == vid.version)
        set_inactive(vid.id);
}

Entity* Entities::get_mut(VID vid) {
    Entity& e = items[vid.id];
    if (e.active && e.vid.version == vid.version)
        return &e;
    return nullptr;
}

const Entity* Entities::get(VID vid) const {
    const Entity& e = items[vid.id];
    if (e.active && e.vid.version == vid.version)
        return &e;
    return nullptr;
}

std::vector<VID> Entities::active_vids() const {
    std::vector<VID> out;
    out.reserve(items.size());
    for (auto const& e : items)
        if (e.active)
            out.push_back(e.vid);
    return out;
}
