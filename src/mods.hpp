#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <filesystem>
#include <chrono>

#include "sprites.hpp"

struct ModInfo {
  std::string name;
  std::string version;
  std::vector<std::string> deps;
  std::string path; // absolute or relative root
};

// Very small mod manager that discovers mods in `mods/`,
// loads `info.toml`, and supports polling-based hot reload
// for `graphics/` and `scripts/` folders.
class ModsManager {
public:
  explicit ModsManager(std::string mods_root);

  // Discover available mods (non-recursive: `mods/*/`).
  void discover_mods();

  // Build a sprite registry from all `graphics/` files across mods.
  // Returns true if registry was rebuilt (e.g., on first call or change).
  bool build_sprite_registry(SpriteIdRegistry& registry);

  // Build rich sprite store by scanning manifests and images.
  // Returns true if store was rebuilt.
  bool build_sprite_store(SpriteStore& store);

  // Poll filesystem for changes and trigger rebuilds as needed.
  // Returns true if anything reloaded.
  bool poll_hot_reload(SpriteIdRegistry& registry, double dt_seconds);
  bool poll_hot_reload(SpriteStore& store, double dt_seconds);

  const std::vector<ModInfo>& mods() const { return mods_; }

private:
  using Clock = std::chrono::steady_clock;
  std::string root_;
  std::vector<ModInfo> mods_;
  std::unordered_map<std::string, std::filesystem::file_time_type> tracked_files_;
  bool registry_built_ = false;
  double accum_poll_ = 0.0; // seconds

  // internal helpers
  static ModInfo parse_info(const std::string& mod_path);
  void track_tree(const std::string& path);
  bool check_changes(std::vector<std::string>& changed_assets,
                     std::vector<std::string>& changed_scripts);
};
