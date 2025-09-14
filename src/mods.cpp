#include "mods.hpp"
#include "globals.hpp"
#include "settings.hpp"
#include "graphics.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <fstream>
#include <system_error>

namespace fs = std::filesystem;

bool init_mods_manager(const std::string& mods_root) {
    ModManager* m = new ModManager{};
    m->root = mods_root;
    mm = m;
    return true;
}

void cleanup_mods_manager() {
    if (mm) {
        delete mm;
        mm = nullptr;
    }
}

/// Discover available mods by scanning `mods/*/` for `info.toml`.
/// Clears any previously discovered mods.
void discover_mods() {
    mm->mods.clear();
    std::error_code ec;
    if (!fs::exists(mm->root, ec) || !fs::is_directory(mm->root, ec))
        return;
    for (auto const& e : fs::directory_iterator(mm->root, ec)) {
        if (ec) {
            ec.clear();
            continue;
        }
        if (!e.is_directory())
            continue;
        auto p = e.path();
        ModInfo mi = mm->parse_info(p.string());
        mm->mods.push_back(std::move(mi));
    }

    // Track initial trees
    mm->tracked_files.clear();
    for (auto const& m : mm->mods) {
        mm->track_tree(m.path + "/graphics");
        mm->track_tree(m.path + "/scripts");
        mm->track_tree(m.path + "/info.toml");
    }
}


/// Re-scan mods and rebuild the sprite name↔id registry.
///
/// Walks each `<mod.path>/graphics` recursively, collects file stems, and
/// namespaces them as `<mod.name>:<stem>`. Skips non-regular files. Clears and
/// ignores transient `std::error_code`s during traversal.
///
/// Calls `rebuild_sprite_registry(names)`, which nukes and replaces:
///   - Graphics::sprite_name_to_id
///   - Graphics::sprite_id_to_name
///
/// Does NOT parse manifests, build SpriteDef data, or load textures.
/// Sets `registry_built = true`.
///
/// ⚠ ID stability: IDs are reassigned based on the collected order, so they may change.
///
/// Returns `true` on completion (does not signal whether anything changed).
/// Complexity: O(#files). Call when you need a fast index refresh after add/remove/rename;
/// use the full store rebuild for manifest/content changes.
bool cheap_scan_mods_to_update_sprite_name_registry() {
    std::vector<std::string> names;
    std::error_code ec;
    for (auto const& m : mm->mods) {
        fs::path gdir = fs::path(m.path) / "graphics";
        if (!fs::exists(gdir, ec) || !fs::is_directory(gdir, ec))
            continue;
        for (auto const& entry : fs::recursive_directory_iterator(gdir, ec)) {
            if (ec) {
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
    build_sprite_name_id_mapping(names);
    mm->registry_built = true;
    std::printf("[mods] Sprite registry built with %zu entries\n", names.size());
    return true;
}

static bool is_manifest_ext(const std::string& ext) {
    return ext == ".sprite" || ext == ".sprite.toml";
}

static bool is_image_ext(const std::string& ext) {
    return ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp" || ext == ".gif" ||
           ext == ".webp" || ext == ".tga";
}

/// Rebuild the full sprite store from all mods.
///
/// Scans each `<mod.path>/graphics` recursively. Prefers manifests
/// (`.sprite`, `.sprite.toml`) over bare images. For each entry:
///   - Parses manifest into `SpriteDef` (namespaced as `<mod.name>:<name>`).
///   - Resolves relative `image_path` against `<mod.path>/graphics/`.
///   - If no manifest exists, synthesizes a default `SpriteDef` from the image.
///
/// Uses first definition per name (later duplicates ignored). Sorts by name
/// for deterministic IDs, then calls `rebuild_sprite_mapping(defs)` which
/// replaces:
///   - `Graphics::sprite_defs_by_id`
///   - `Graphics::sprite_id_to_name`
///   - `Graphics::sprite_name_to_id`
///
/// Does NOT load textures. Log-and-continue on parse errors. Complexity
/// O(files + parse). Call on startup and whenever manifests or image content
/// change. IDs may change if the name set changes.
bool scan_mods_for_sprite_defs() {
    auto mod_infos = mm->mods;
    
    // First pass: collect manifests per name (prefer manifests over bare images)
    std::unordered_map<std::string, SpriteDef> defs_by_name;
    std::unordered_map<std::string, std::string>
        name_to_modroot; // for resolving relative image paths
    std::error_code ec;
    for (auto const& m : mod_infos) {
        fs::path gdir = fs::path(m.path) / "graphics";
        if (!fs::exists(gdir, ec) || !fs::is_directory(gdir, ec))
            continue;
        for (auto const& entry : fs::recursive_directory_iterator(gdir, ec)) {
            if (ec) {
                ec.clear();
                continue;
            }
            if (!entry.is_regular_file())
                continue;
            auto p = entry.path();
            std::string ext = p.extension().string();
            for (auto& c : ext)
                c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            if (is_manifest_ext(ext)) {
                SpriteDef def{};
                std::string err;
                if (parse_sprite_manifest_file(p.string(), def, err)) {
                    // Namespace the sprite name if missing a prefix
                    if (def.name.find(':') == std::string::npos) {
                        def.name = m.name + ":" + def.name;
                    }
                    // Resolve image path relative to mod root if not absolute
                    if (!def.image_path.empty() && !fs::path(def.image_path).is_absolute()) {
                        def.image_path = (fs::path(m.path) / "graphics" / def.image_path)
                                             .lexically_normal()
                                             .string();
                    }
                    // Keep first definition for a name; later mods can override if desired (could
                    // be policy)
                    if (defs_by_name.find(def.name) == defs_by_name.end()) {
                        defs_by_name.emplace(def.name, std::move(def));
                        name_to_modroot[defs_by_name.find(def.name)->first] = m.path;
                    }
                } else {
                    std::printf("[mods] Sprite manifest parse failed: %s (%s)\n",
                                p.string().c_str(), err.c_str());
                }
            }
        }
    }
    // Second pass: images without manifests become default single-frame sprites
    for (auto const& m : mod_infos) {
        fs::path gdir = fs::path(m.path) / "graphics";
        if (!fs::exists(gdir, ec) || !fs::is_directory(gdir, ec))
            continue;
        for (auto const& entry : fs::recursive_directory_iterator(gdir, ec)) {
            if (ec) {
                ec.clear();
                continue;
            }
            if (!entry.is_regular_file())
                continue;
            auto p = entry.path();
            std::string ext = p.extension().string();
            for (auto& c : ext)
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
    for (auto& kv : defs_by_name)
        sorted.emplace_back(kv.first, std::move(kv.second));
    std::sort(sorted.begin(), sorted.end(),
              [](const auto& a, const auto& b) { return a.first < b.first; });
    std::vector<SpriteDef> defs;
    defs.reserve(sorted.size());
    for (auto& kv : sorted)
        defs.push_back(std::move(kv.second));
 
    rebuild_sprite_mapping(defs);
    std::printf("[mods] Sprite store built with %zu entries\n", defs.size());
    return true;
}


bool poll_fs_mods_hot_reload() {
    mm->accum_poll += ss->dt;
    if (mm->accum_poll < HOT_RELOAD_POLL_INTERVAL)
        return false;
    mm->accum_poll = 0.0f;

    std::vector<std::string> changed_assets;
    std::vector<std::string> changed_scripts;
    bool any = mm->check_changes(changed_assets, changed_scripts);
    if (!any)
        return false;

    if (!changed_assets.empty()) {
        std::printf("[mods] Asset changes detected (%zu). Rebuilding sprites...\n",
                    changed_assets.size());
        scan_mods_for_sprite_defs();
    }
    if (!changed_scripts.empty()) {
        std::printf("[mods] Script changes detected (%zu). Reloading Lua...\n",
                    changed_scripts.size());
        if (luam) {
            luam->load_mods();
            // Update existing entities from defs (sprite, sizes, and stats)
            for (auto& e : ss->entities.data()) {
                if (!e.active || e.def_type == 0) continue;
                if (const auto* ed = luam->find_entity_type(e.def_type)) {
                    // Visuals and collider
                    e.sprite_id = -1;
                    if (!ed->sprite.empty() && ed->sprite.find(':') != std::string::npos)
                        e.sprite_id = try_get_sprite_id(ed->sprite);
                    e.sprite_size = {ed->sprite_w, ed->sprite_h};
                    e.size = {ed->collider_w, ed->collider_h};
                    // Preserve ratios when changing caps
                    float hp_ratio = (e.max_hp > 0) ? ((float)e.health / (float)e.max_hp) : 0.0f;
                    float sh_ratio = (e.stats.shield_max > 0.0f) ? (e.shield / e.stats.shield_max) : 0.0f;
                    // Core caps and regen
                    e.max_hp = ed->max_hp;
                    e.health = (uint32_t)std::lround(hp_ratio * (float)e.max_hp);
                    e.stats.health_regen = ed->health_regen;
                    e.stats.shield_max = ed->shield_max;
                    e.shield = ed->shield_max * sh_ratio;
                    e.stats.shield_regen = ed->shield_regen;
                    // Defense and movement
                    e.stats.armor = ed->armor;
                    e.stats.plates = ed->plates;
                    e.stats.move_speed = ed->move_speed;
                    e.stats.dodge = ed->dodge;
                    // Economy/modifiers
                    e.stats.scavenging = ed->scavenging;
                    e.stats.currency = ed->currency;
                    e.stats.ammo_gain = ed->ammo_gain;
                    e.stats.luck = ed->luck;
                    // Combat
                    e.stats.crit_chance = ed->crit_chance;
                    e.stats.crit_damage = ed->crit_damage;
                    e.stats.headshot_damage = ed->headshot_damage;
                    e.stats.damage_absorb = ed->damage_absorb;
                    e.stats.damage_output = ed->damage_output;
                    e.stats.healing = ed->healing;
                    e.stats.accuracy = ed->accuracy;
                    e.stats.terror_level = ed->terror_level;
                    // Movement spread dynamics
                    e.stats.move_spread_inc_rate_deg_per_sec_at_base = ed->move_spread_inc_rate_deg_per_sec_at_base;
                    e.stats.move_spread_decay_deg_per_sec = ed->move_spread_decay_deg_per_sec;
                    e.stats.move_spread_max_deg = ed->move_spread_max_deg;
                }
            }
            ss->alerts.push_back({"Lua reloaded", 0.0f, 1.5f, false});
        }
    }
    return true;
}
