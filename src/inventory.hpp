#pragma once

#include <vector>
#include <optional>
#include <algorithm>
#include "items.hpp"

inline constexpr std::size_t INV_MAX_SLOTS = 10;

struct InvEntry {
  std::size_t index{0};
  Item item{};
};

struct Inventory {
  std::vector<InvEntry> entries{};
  std::size_t selected_index{0};

  static Inventory make() { return {}; }

  bool is_full() const { return entries.size() >= INV_MAX_SLOTS; }
  bool is_empty() const { return entries.empty(); }

  const InvEntry* get(std::size_t idx) const {
    auto it = std::find_if(entries.begin(), entries.end(), [&](auto const& e){ return e.index == idx; });
    return (it==entries.end()) ? nullptr : &*it;
  }
  InvEntry* get_mut(std::size_t idx) {
    auto it = std::find_if(entries.begin(), entries.end(), [&](auto const& e){ return e.index == idx; });
    return (it==entries.end()) ? nullptr : &*it;
  }
  const InvEntry* selected_entry() const { return get(selected_index); }
  InvEntry* selected_entry_mut() { return get_mut(selected_index); }

  // Insert with stack-first, then empty slot (prefer selected), then swap selected if full.
  std::optional<Item> insert(Item item_to_add) {
    if (item_to_add.is_stackable()) {
      for (auto& e : entries) {
        if (e.item.type == item_to_add.type && e.item.count < e.item.max_count) {
          uint32_t space = e.item.max_count - e.item.count;
          uint32_t xfer = std::min(space, item_to_add.count);
          e.item.count += xfer; item_to_add.count -= xfer;
          if (item_to_add.count == 0) return std::nullopt;
        }
      }
    }
    if (!is_full()) {
      bool selected_empty = (get(selected_index) == nullptr);
      if (selected_empty) {
        entries.push_back(InvEntry{selected_index, item_to_add});
        std::sort(entries.begin(), entries.end(), [](auto const& a, auto const& b){ return a.index < b.index; });
        return std::nullopt;
      }
      // find any empty slot
      for (std::size_t i=0;i<INV_MAX_SLOTS;++i) {
        if (get(i) == nullptr) {
          entries.push_back(InvEntry{i, item_to_add});
          std::sort(entries.begin(), entries.end(), [](auto const& a, auto const& b){ return a.index < b.index; });
          return std::nullopt;
        }
      }
    }
    // full: swap with selected
    if (auto* se = get_mut(selected_index)) { Item old = se->item; se->item = item_to_add; return old; }
    return item_to_add; // fallback
  }

  void remove_count_from_slot(std::size_t idx, uint32_t count) {
    if (auto* e = get_mut(idx)) {
      if (e->item.count > count) { e->item.count -= count; }
      else { entries.erase(std::remove_if(entries.begin(), entries.end(), [&](auto const& x){ return x.index == idx; }), entries.end()); }
    }
  }

  void set_selected_index(std::size_t idx) { if (idx < INV_MAX_SLOTS) selected_index = idx; }
  void increment_selected_index() { selected_index = (selected_index + 1) % INV_MAX_SLOTS; }
  void decrement_selected_index() { selected_index = (selected_index + INV_MAX_SLOTS - 1) % INV_MAX_SLOTS; }
};

