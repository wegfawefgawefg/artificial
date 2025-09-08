#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include "types.hpp"

template <typename T, std::size_t N>
class Pool {
public:
  Pool() {
    versions_.fill(1);
  }

  std::size_t capacity() const { return N; }

  std::optional<VID> alloc() {
    for (std::size_t i = 0; i < N; ++i) {
      if (!items_[i].active) {
        items_[i] = T{};
        items_[i].active = true;
        return VID{ i, versions_[i] };
      }
    }
    return std::nullopt;
  }

  void free(VID v) {
    if (v.id >= N) return;
    if (versions_[v.id] != v.version) return;
    items_[v.id].active = false;
    versions_[v.id] += 1; // invalidate stale refs
  }

  T* get(VID v) {
    if (v.id >= N) return nullptr;
    if (versions_[v.id] != v.version) return nullptr;
    if (!items_[v.id].active) return nullptr;
    return &items_[v.id];
  }

  const T* get(VID v) const {
    if (v.id >= N) return nullptr;
    if (versions_[v.id] != v.version) return nullptr;
    if (!items_[v.id].active) return nullptr;
    return &items_[v.id];
  }

  // Raw access (useful for iteration/cleanup)
  std::array<T, N>& data() { return items_; }
  const std::array<T, N>& data() const { return items_; }

private:
  std::array<T, N> items_{};
  std::array<std::uint32_t, N> versions_{};
};

