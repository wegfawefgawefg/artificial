#include "mods.hpp"
#include "settings.hpp"

#include <system_error>
#include <fstream>
#include <cstdio>
#include <cctype>
#include <algorithm>

namespace fs = std::filesystem;

static std::string trim(const std::string &s)
{
  size_t a = 0, b = s.size();
  while (a < b && std::isspace(static_cast<unsigned char>(s[a])))
    ++a;
  while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1])))
    --b;
  return s.substr(a, b - a);
}

static bool starts_with(const std::string &s, const char *pfx)
{
  return s.rfind(pfx, 0) == 0;
}

ModsManager::ModsManager(std::string mods_root)
    : root_(std::move(mods_root)) {}

ModInfo ModsManager::parse_info(const std::string &mod_path)
{
  ModInfo info{};
  info.path = mod_path;
  std::ifstream f(mod_path + "/info.toml");
  if (!f.good())
  {
    // derive name from folder leaf
    info.name = fs::path(mod_path).filename().string();
    info.version = "0.0.0";
    return info;
  }
  std::string line;
  while (std::getline(f, line))
  {
    std::string t = trim(line);
    if (t.empty() || t[0] == '#')
      continue;
    if (starts_with(t, "name"))
    {
      auto pos = t.find('=');
      if (pos != std::string::npos)
      {
        std::string v = trim(t.substr(pos + 1));
        if (!v.empty() && v.front() == '"' && v.back() == '"')
          v = v.substr(1, v.size() - 2);
        info.name = v;
      }
    }
    else if (starts_with(t, "version"))
    {
      auto pos = t.find('=');
      if (pos != std::string::npos)
      {
        std::string v = trim(t.substr(pos + 1));
        if (!v.empty() && v.front() == '"' && v.back() == '"')
          v = v.substr(1, v.size() - 2);
        info.version = v;
      }
    }
    else if (starts_with(t, "deps"))
    {
      auto pos = t.find('=');
      if (pos != std::string::npos)
      {
        std::string v = trim(t.substr(pos + 1));
        // parse ["a","b"] minimally
        if (!v.empty() && v.front() == '[' && v.back() == ']')
        {
          std::string inner = v.substr(1, v.size() - 2);
          size_t i = 0;
          while (i < inner.size())
          {
            while (i < inner.size() && std::isspace(static_cast<unsigned char>(inner[i])))
              ++i;
            if (i < inner.size() && inner[i] == '"')
            {
              ++i;
              size_t j = i;
              while (j < inner.size() && inner[j] != '"')
                ++j;
              if (j < inner.size())
              {
                info.deps.push_back(inner.substr(i, j - i));
                i = j + 1;
              }
            }
            while (i < inner.size() && inner[i] != ',')
              ++i;
            if (i < inner.size() && inner[i] == ',')
              ++i;
          }
        }
      }
    }
  }
  if (info.name.empty())
    info.name = fs::path(mod_path).filename().string();
  if (info.version.empty())
    info.version = "0.0.0";
  return info;
}

void ModsManager::discover_mods()
{
  mods_.clear();
  std::error_code ec;
  if (!fs::exists(root_, ec) || !fs::is_directory(root_, ec))
    return;
  for (auto const &e : fs::directory_iterator(root_, ec))
  {
    if (ec)
    {
      ec.clear();
      continue;
    }
    if (!e.is_directory())
      continue;
    auto p = e.path();
    ModInfo mi = parse_info(p.string());
    mods_.push_back(std::move(mi));
  }

  // Track initial trees
  tracked_files_.clear();
  for (auto const &m : mods_)
  {
    track_tree(m.path + "/graphics");
    track_tree(m.path + "/scripts");
    track_tree(m.path + "/info.toml");
  }
}

void ModsManager::track_tree(const std::string &path)
{
  std::error_code ec;
  fs::path p(path);
  if (!fs::exists(p, ec))
    return;
  if (fs::is_regular_file(p, ec))
  {
    tracked_files_[p.string()] = fs::last_write_time(p, ec);
    return;
  }
  if (!fs::is_directory(p, ec))
    return;
  for (auto const &entry : fs::recursive_directory_iterator(p, ec))
  {
    if (ec)
    {
      ec.clear();
      continue;
    }
    if (entry.is_regular_file())
    {
      auto sp = entry.path().string();
      tracked_files_[sp] = fs::last_write_time(entry.path(), ec);
    }
  }
}

bool ModsManager::build_sprite_registry(SpriteIdRegistry &registry)
{
  std::vector<std::string> names;
  std::error_code ec;
  for (auto const &m : mods_)
  {
    fs::path gdir = fs::path(m.path) / "graphics";
    if (!fs::exists(gdir, ec) || !fs::is_directory(gdir, ec))
      continue;
    for (auto const &entry : fs::recursive_directory_iterator(gdir, ec))
    {
      if (ec)
      {
        ec.clear();
        continue;
      }
      if (!entry.is_regular_file())
        continue;
      auto stem = entry.path().stem().string();
      if (!stem.empty()) {
        std::string ns = m.name + ":" + stem;
        names.push_back(ns);
      }
    }
  }
  registry.rebuild_from(names);
  registry_built_ = true;
  std::printf("[mods] Sprite registry built with %d entries\n", registry.size());
  return true;
}

static bool is_manifest_ext(const std::string &ext)
{
  return ext == ".sprite" || ext == ".sprite.toml";
}

static bool is_image_ext(const std::string &ext)
{
  return ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp" || ext == ".gif" || ext == ".webp" || ext == ".tga";
}

bool ModsManager::build_sprite_store(SpriteStore &store)
{
  // First pass: collect manifests per name (prefer manifests over bare images)
  std::unordered_map<std::string, SpriteDef> defs_by_name;
  std::unordered_map<std::string, std::string> name_to_modroot; // for resolving relative image paths
  std::error_code ec;
  for (auto const &m : mods_)
  {
    fs::path gdir = fs::path(m.path) / "graphics";
    if (!fs::exists(gdir, ec) || !fs::is_directory(gdir, ec))
      continue;
    for (auto const &entry : fs::recursive_directory_iterator(gdir, ec))
    {
      if (ec)
      {
        ec.clear();
        continue;
      }
      if (!entry.is_regular_file())
        continue;
      auto p = entry.path();
      std::string ext = p.extension().string();
      for (auto &c : ext)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
      if (is_manifest_ext(ext))
      {
        SpriteDef def{};
        std::string err;
        if (parse_sprite_manifest_file(p.string(), def, err))
        {
          // Namespace the sprite name if missing a prefix
          if (def.name.find(':') == std::string::npos) {
            def.name = m.name + ":" + def.name;
          }
          // Resolve image path relative to mod root if not absolute
          if (!def.image_path.empty() && !fs::path(def.image_path).is_absolute())
          {
            def.image_path = (fs::path(m.path) / "graphics" / def.image_path).lexically_normal().string();
          }
          // Keep first definition for a name; later mods can override if desired (could be policy)
          if (defs_by_name.find(def.name) == defs_by_name.end())
          {
            defs_by_name.emplace(def.name, std::move(def));
            name_to_modroot[defs_by_name.find(def.name)->first] = m.path;
          }
        }
        else
        {
          std::printf("[mods] Sprite manifest parse failed: %s (%s)\n", p.string().c_str(), err.c_str());
        }
      }
    }
  }
  // Second pass: images without manifests become default single-frame sprites
  for (auto const &m : mods_)
  {
    fs::path gdir = fs::path(m.path) / "graphics";
    if (!fs::exists(gdir, ec) || !fs::is_directory(gdir, ec))
      continue;
    for (auto const &entry : fs::recursive_directory_iterator(gdir, ec))
    {
      if (ec)
      {
        ec.clear();
        continue;
      }
      if (!entry.is_regular_file())
        continue;
      auto p = entry.path();
      std::string ext = p.extension().string();
      for (auto &c : ext)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
      if (!is_image_ext(ext))
        continue;
      std::string stem = p.stem().string();
      std::string nsname = m.name + ":" + stem;
      if (defs_by_name.find(nsname) != defs_by_name.end())
        continue; // manifest already defined
      SpriteDef d = make_default_sprite_from_image(nsname, p.string());
      defs_by_name.emplace(nsname, std::move(d));
      name_to_modroot[nsname] = m.path;
    }
  }

  // Deterministic ordering: sort by name before rebuilding
  std::vector<std::pair<std::string, SpriteDef>> sorted;
  sorted.reserve(defs_by_name.size());
  for (auto &kv : defs_by_name)
    sorted.emplace_back(kv.first, std::move(kv.second));
  std::sort(sorted.begin(), sorted.end(), [](const auto &a, const auto &b)
            { return a.first < b.first; });
  std::vector<SpriteDef> defs;
  defs.reserve(sorted.size());
  for (auto &kv : sorted)
    defs.push_back(std::move(kv.second));

  store.rebuild_from(defs);
  std::printf("[mods] Sprite store built with %d entries\n", store.size());
  return true;
}

bool ModsManager::check_changes(std::vector<std::string> &changed_assets,
                                std::vector<std::string> &changed_scripts)
{
  bool any = false;
  std::error_code ec;
  // Re-scan tracked files and detect new/removed/modified
  std::unordered_map<std::string, fs::file_time_type> current;
  current.reserve(tracked_files_.size());
  for (auto const &m : mods_)
  {
    // info.toml
    fs::path ip = fs::path(m.path) / "info.toml";
    if (fs::exists(ip, ec))
      current[ip.string()] = fs::last_write_time(ip, ec);
    // graphics
    fs::path gdir = fs::path(m.path) / "graphics";
    if (fs::exists(gdir, ec) && fs::is_directory(gdir, ec))
    {
      for (auto const &e : fs::recursive_directory_iterator(gdir, ec))
      {
        if (ec)
        {
          ec.clear();
          continue;
        }
        if (e.is_regular_file())
          current[e.path().string()] = fs::last_write_time(e.path(), ec);
      }
    }
    // scripts
    fs::path sdir = fs::path(m.path) / "scripts";
    if (fs::exists(sdir, ec) && fs::is_directory(sdir, ec))
    {
      for (auto const &e : fs::recursive_directory_iterator(sdir, ec))
      {
        if (ec)
        {
          ec.clear();
          continue;
        }
        if (e.is_regular_file())
          current[e.path().string()] = fs::last_write_time(e.path(), ec);
      }
    }
  }

  // Compare with previous snapshot
  for (auto const &[path, ts] : current)
  {
    auto it = tracked_files_.find(path);
    if (it == tracked_files_.end())
    {
      any = true;
      if (path.find("/graphics/") != std::string::npos)
        changed_assets.push_back(path);
      else if (path.find("/scripts/") != std::string::npos)
        changed_scripts.push_back(path);
      else if (path.rfind("info.toml") != std::string::npos)
      { /* ignore for now */
      }
    }
    else if (ts != it->second)
    {
      any = true;
      if (path.find("/graphics/") != std::string::npos)
        changed_assets.push_back(path);
      else if (path.find("/scripts/") != std::string::npos)
        changed_scripts.push_back(path);
      else if (path.rfind("info.toml") != std::string::npos)
      { /* ignore for now */
      }
    }
  }
  // Detect deletions
  for (auto const &[path, _] : tracked_files_)
  {
    if (current.find(path) == current.end())
    {
      any = true;
      if (path.find("/graphics/") != std::string::npos)
        changed_assets.push_back(path);
      else if (path.find("/scripts/") != std::string::npos)
        changed_scripts.push_back(path);
    }
  }
  // Update snapshot
  tracked_files_ = std::move(current);
  return any;
}

bool ModsManager::poll_hot_reload(SpriteIdRegistry &registry, double dt_seconds)
{
  accum_poll_ += dt_seconds;
  if (accum_poll_ < static_cast<double>(HOT_RELOAD_POLL_INTERVAL))
    return false;
  accum_poll_ = 0.0;

  std::vector<std::string> changed_assets;
  std::vector<std::string> changed_scripts;
  bool any = check_changes(changed_assets, changed_scripts);
  if (!any)
    return false;

  if (!changed_assets.empty())
  {
    std::printf("[mods] Asset changes detected (%zu). Rebuilding sprites...\n", changed_assets.size());
    build_sprite_registry(registry);
  }
  if (!changed_scripts.empty())
  {
    std::printf("[mods] Script changes detected (%zu). Reloading behaviors (stub).\n", changed_scripts.size());
    // Placeholder: once Lua is integrated, reload affected script modules here.
  }
  return true;
}

bool ModsManager::poll_hot_reload(SpriteStore &store, double dt_seconds)
{
  accum_poll_ += dt_seconds;
  if (accum_poll_ < static_cast<double>(HOT_RELOAD_POLL_INTERVAL))
    return false;
  accum_poll_ = 0.0;

  std::vector<std::string> changed_assets;
  std::vector<std::string> changed_scripts;
  bool any = check_changes(changed_assets, changed_scripts);
  if (!any)
    return false;

  if (!changed_assets.empty())
  {
    std::printf("[mods] Asset changes detected (%zu). Rebuilding sprite store...\n", changed_assets.size());
    build_sprite_store(store);
  }
  if (!changed_scripts.empty())
  {
    std::printf("[mods] Script changes detected (%zu). Reloading behaviors (stub).\n", changed_scripts.size());
  }
  return true;
}
