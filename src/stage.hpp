#pragma once

#include <cstdint>
#include <vector>

struct TileProps {
  // bit0: blocks_entities, bit1: blocks_projectiles
  // walkable == !blocks_entities
  std::uint8_t flags{0};
  inline bool blocks_entities() const { return (flags & 0x1) != 0; }
  inline bool blocks_projectiles() const { return (flags & 0x2) != 0; }
  static TileProps Make(bool block_e, bool block_p) {
    TileProps t{}; t.flags = (block_e ? 0x1 : 0) | (block_p ? 0x2 : 0); return t;
  }
};

class Stage {
public:
  Stage(std::uint32_t w = 64, std::uint32_t h = 36) : width(w), height(h) {
    tiles.resize(width * height);
  }

  std::uint32_t get_width() const { return width; }
  std::uint32_t get_height() const { return height; }

  bool in_bounds(int x, int y) const {
    return x >= 0 && y >= 0 && (std::uint32_t)x < width && (std::uint32_t)y < height;
  }

  TileProps& at(int x, int y) { return tiles[(std::uint32_t)y * width + (std::uint32_t)x]; }
  const TileProps& at(int x, int y) const { return tiles[(std::uint32_t)y * width + (std::uint32_t)x]; }

  void fill_border(TileProps t) {
    for (std::uint32_t x = 0; x < width; ++x) { at((int)x, 0) = t; at((int)x, (int)height-1) = t; }
    for (std::uint32_t y = 0; y < height; ++y) { at(0, (int)y) = t; at((int)width-1, (int)y) = t; }
  }

private:
  std::uint32_t width;
  std::uint32_t height;
  std::vector<TileProps> tiles;
};

