#pragma once

#include <string>
#include <unordered_map>
#include <vector>

// Frame data for a sprite. duration_sec == 0 when using global fps.
struct SpriteFrame {
    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;
    float duration_sec = 0.0f;
};

// Sprite definition describing how to render an image (static or animated).
// Note: Rendering system is deferred; this only holds metadata.
struct SpriteDef {
    std::string name;
    std::string image_path; // absolute or relative path
    std::vector<SpriteFrame> frames;
    bool loop = false;
    float fps = 0.0f; // 0 => use per-frame durations
    // Pivot in pixel space. (-1,-1) means "auto" (frame center when known).
    int pivot_px_x = -1;
    int pivot_px_y = -1;
    // Optional world-space offset applied at render time.
    float world_offset_x = 0.0f;
    float world_offset_y = 0.0f;
};

// Minimal sprite registry stub: maps sprite names (e.g., filenames without extension)
// to compact integer IDs. No textures are loaded yet; rendering still uses debug rects.
struct SpriteIdRegistry {
    // Returns existing ID or assigns a new one
    int get_or_add(const std::string& name);
    // Clears and rebuilds from a list of names
    void rebuild_from(const std::vector<std::string>& names);
    // Retrieve ID if present, else -1
    int try_get(const std::string& name) const;
    // Total registered count
    int size() const {
        return static_cast<int>(name_to_id.size());
    }
    // Returns a stable vector of names ordered by ID (for debugging/UI).
    const std::vector<std::string>& names_by_id() const {
        return id_to_name;
    }

  private:
    std::unordered_map<std::string, int> name_to_id;
    std::vector<std::string> id_to_name; // index = id
};

// Rich sprite store that absorbs name<->ID mapping and holds SpriteDef metadata.
class SpriteStore {
  public:
    // Rebuild store from new definitions.
    // If only additions are detected (no removals), preserve existing IDs.
    void rebuild_from(const std::vector<SpriteDef>& new_defs);

    // Lookup utilities
    int try_get_id(const std::string& name) const;
    const SpriteDef* try_get_def(const std::string& name) const;
    const SpriteDef* get_def_by_id(int id) const;

    int size() const {
        return static_cast<int>(defs_by_id.size());
    }
    const std::vector<SpriteDef>& defs() const {
        return defs_by_id;
    }

    const std::vector<std::string>& names_by_id() const {
        return id_to_name;
    }

  private:
    std::unordered_map<std::string, int> name_to_id;
    std::vector<std::string> id_to_name;
    std::vector<SpriteDef> defs_by_id;
};

// Parsing helpers (minimal tolerant parser for sidecar manifests)
// Returns true on success; on failure, out_error receives a message.
bool parse_sprite_manifest_file(const std::string& path, SpriteDef& out_def,
                                std::string& out_error);

// Build a default single-frame sprite definition for an image without manifest.
SpriteDef make_default_sprite_from_image(const std::string& name, const std::string& image_path);
