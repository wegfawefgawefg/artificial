#pragma once

#include "types.hpp"

#include <algorithm>
#include <optional>
#include <vector>

inline constexpr std::size_t INV_MAX_SLOTS = 10;

enum InvKind { INV_ITEM = 1, INV_GUN = 2 };

struct InvEntry {
    std::size_t index{0};
    int kind{INV_ITEM};
    VID vid{};
};

struct Inventory {
    std::vector<InvEntry> entries{};
    std::size_t selected_index{0};

    static Inventory make() {
        return {};
    }

    bool is_full() const {
        return entries.size() >= INV_MAX_SLOTS;
    }
    bool is_empty() const {
        return entries.empty();
    }

    const InvEntry* get(std::size_t idx) const {
        auto it = std::find_if(entries.begin(), entries.end(),
                               [&](auto const& e) { return e.index == idx; });
        return (it == entries.end()) ? nullptr : &*it;
    }
    InvEntry* get_mut(std::size_t idx) {
        auto it = std::find_if(entries.begin(), entries.end(),
                               [&](auto const& e) { return e.index == idx; });
        return (it == entries.end()) ? nullptr : &*it;
    }
    const InvEntry* selected_entry() const {
        return get(selected_index);
    }
    InvEntry* selected_entry_mut() {
        return get_mut(selected_index);
    }

    // Insert an existing instance by VID (kind = item or gun). Fills selected if empty, else first
    // empty, else fails.
    bool insert_existing(int kind, VID vid) {
        if (get(selected_index) == nullptr) {
            entries.push_back(InvEntry{selected_index, kind, vid});
            std::sort(entries.begin(), entries.end(),
                      [](auto const& a, auto const& b) { return a.index < b.index; });
            return true;
        }
        for (std::size_t i = 0; i < INV_MAX_SLOTS; ++i) {
            if (get(i) == nullptr) {
                entries.push_back(InvEntry{i, kind, vid});
                std::sort(entries.begin(), entries.end(),
                          [](auto const& a, auto const& b) { return a.index < b.index; });
                return true;
            }
        }
        return false;
    }

    void remove_slot(std::size_t idx) {
        entries.erase(std::remove_if(entries.begin(), entries.end(),
                                     [&](auto const& x) { return x.index == idx; }),
                      entries.end());
    }

    void set_selected_index(std::size_t idx) {
        if (idx < INV_MAX_SLOTS)
            selected_index = idx;
    }
    void increment_selected_index() {
        selected_index = (selected_index + 1) % INV_MAX_SLOTS;
    }
    void decrement_selected_index() {
        selected_index = (selected_index + INV_MAX_SLOTS - 1) % INV_MAX_SLOTS;
    }
};
