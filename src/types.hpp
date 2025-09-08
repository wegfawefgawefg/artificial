#pragma once

#include <cstdint>
#include <cstddef>

// Integer IDs instead of enums (moddable via data tables later)
namespace ids {
inline constexpr int MODE_TITLE = 0;
inline constexpr int MODE_SETTINGS = 1;
inline constexpr int MODE_VIDEO_SETTINGS = 2;
inline constexpr int MODE_PLAYING = 3;
inline constexpr int MODE_GAME_OVER = 4;
inline constexpr int MODE_WIN = 5;
inline constexpr int MODE_SCORE_REVIEW = 6;

inline constexpr int ET_NONE = 0;
inline constexpr int ET_PLAYER = 1;
inline constexpr int ET_NPC = 2;
inline constexpr int ET_ITEM = 3;
}

struct VID {
  std::size_t id{0};
  std::uint32_t version{0};
};
