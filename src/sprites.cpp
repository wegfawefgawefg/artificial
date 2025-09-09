#include "sprites.hpp"

#include <cctype>
#include <cstdio>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

int SpriteIdRegistry::get_or_add(const std::string& name) {
    auto it = name_to_id.find(name);
    if (it != name_to_id.end())
        return it->second;
    int id = static_cast<int>(id_to_name.size());
    name_to_id.emplace(name, id);
    id_to_name.push_back(name);
    return id;
}

void SpriteIdRegistry::rebuild_from(const std::vector<std::string>& names) {
    name_to_id.clear();
    id_to_name.clear();
    id_to_name.reserve(names.size());
    for (const auto& n : names) {
        int id = static_cast<int>(id_to_name.size());
        name_to_id.emplace(n, id);
        id_to_name.push_back(n);
    }
}

int SpriteIdRegistry::try_get(const std::string& name) const {
    auto it = name_to_id.find(name);
    return (it == name_to_id.end()) ? -1 : it->second;
}

// ---------------- SpriteStore ----------------

static std::string trim(const std::string& s) {
    size_t a = 0, b = s.size();
    while (a < b && std::isspace(static_cast<unsigned char>(s[a])))
        ++a;
    while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1])))
        --b;
    return s.substr(a, b - a);
}

void SpriteStore::rebuild_from(const std::vector<SpriteDef>& new_defs) {
    // Determine if there are removals compared to current map.
    bool only_additions = true;
    if (!name_to_id.empty()) {
        // Build set of new names
        std::unordered_map<std::string, int> new_names;
        new_names.reserve(new_defs.size());
        for (const auto& d : new_defs)
            new_names.emplace(d.name, 1);
        for (const auto& kv : name_to_id) {
            if (new_names.find(kv.first) == new_names.end()) {
                only_additions = false;
                break;
            }
        }
    }

    std::unordered_map<std::string, int> new_name_to_id;
    std::vector<std::string> new_id_to_name;
    std::vector<SpriteDef> new_defs_by_id;
    new_name_to_id.reserve(new_defs.size());
    new_id_to_name.reserve(new_defs.size());
    new_defs_by_id.resize(0);

    if (!name_to_id.empty() && only_additions) {
        // Preserve existing IDs; place existing first at their current indices.
        new_defs_by_id = std::vector<SpriteDef>(defs_by_id.size());
        new_id_to_name = id_to_name; // copy
        // Map existing names to their positions, overwrite with updated defs.
        for (const auto& d : new_defs) {
            auto it = name_to_id.find(d.name);
            if (it != name_to_id.end()) {
                int id = it->second;
                if (id >= 0 && static_cast<size_t>(id) < new_defs_by_id.size()) {
                    new_defs_by_id[static_cast<size_t>(id)] = d;
                    new_name_to_id.emplace(d.name, id);
                }
            }
        }
        // Append new names (those not present before) with fresh IDs.
        for (const auto& d : new_defs) {
            if (new_name_to_id.find(d.name) != new_name_to_id.end())
                continue;
            int id = static_cast<int>(new_defs_by_id.size());
            new_defs_by_id.push_back(d);
            new_id_to_name.push_back(d.name);
            new_name_to_id.emplace(d.name, id);
        }
    } else {
        // Re-pack: assign IDs sequentially by input order.
        new_defs_by_id.reserve(new_defs.size());
        for (const auto& d : new_defs) {
            int id = static_cast<int>(new_defs_by_id.size());
            new_defs_by_id.push_back(d);
            new_name_to_id.emplace(d.name, id);
            new_id_to_name.push_back(d.name);
        }
    }

    name_to_id.swap(new_name_to_id);
    id_to_name.swap(new_id_to_name);
    defs_by_id.swap(new_defs_by_id);
}

int SpriteStore::try_get_id(const std::string& name) const {
    auto it = name_to_id.find(name);
    return (it == name_to_id.end()) ? -1 : it->second;
}

const SpriteDef* SpriteStore::try_get_def(const std::string& name) const {
    int id = try_get_id(name);
    if (id < 0)
        return nullptr;
    return get_def_by_id(id);
}

const SpriteDef* SpriteStore::get_def_by_id(int id) const {
    if (id < 0)
        return nullptr;
    size_t idx = static_cast<size_t>(id);
    if (idx >= defs_by_id.size())
        return nullptr;
    return &defs_by_id[idx];
}

// --------------- Manifest parsing ---------------

static bool parse_bool(const std::string& v, bool& out) {
    if (v == "true" || v == "True" || v == "TRUE") {
        out = true;
        return true;
    }
    if (v == "false" || v == "False" || v == "FALSE") {
        out = false;
        return true;
    }
    return false;
}

static bool parse_int(const std::string& v, int& out) {
    try {
        size_t p = 0;
        long val = std::stol(v, &p, 10);
        if (p != v.size())
            return false;
        out = static_cast<int>(val);
        return true;
    } catch (...) {
        return false;
    }
}

static bool parse_float(const std::string& v, float& out) {
    try {
        size_t p = 0;
        double val = std::stod(v, &p);
        if (p != v.size())
            return false;
        out = static_cast<float>(val);
        return true;
    } catch (...) {
        return false;
    }
}

static std::vector<std::string> split_csv(const std::string& s) {
    std::vector<std::string> out;
    std::string cur;
    int depth = 0;
    for (char c : s) {
        if (c == '[' || c == '{' || c == '(') {
            depth++;
            cur.push_back(c);
        } else if (c == ']' || c == '}' || c == ')') {
            depth--;
            cur.push_back(c);
        } else if (c == ',' && depth == 0) {
            out.push_back(trim(cur));
            cur.clear();
        } else {
            cur.push_back(c);
        }
    }
    if (!cur.empty())
        out.push_back(trim(cur));
    return out;
}

static bool parse_int_pair(const std::string& arr, int& a, int& b) {
    std::string s = trim(arr);
    if (s.size() < 2 || s.front() != '[' || s.back() != ']')
        return false;
    std::string inner = trim(s.substr(1, s.size() - 2));
    auto parts = split_csv(inner);
    if (parts.size() != 2)
        return false;
    return parse_int(parts[0], a) && parse_int(parts[1], b);
}

SpriteDef make_default_sprite_from_image(const std::string& name, const std::string& image_path) {
    SpriteDef d{};
    d.name = name;
    d.image_path = image_path;
    d.loop = false;
    d.fps = 0.0f;
    // Unknown image size at this layer; use a placeholder frame rect.
    SpriteFrame f{};
    f.x = 0;
    f.y = 0;
    f.w = 0;
    f.h = 0;
    f.duration_sec = 0.0f;
    d.frames.push_back(f);
    return d;
}

bool parse_sprite_manifest_file(const std::string& path, SpriteDef& out_def,
                                std::string& out_error) {
    std::ifstream f(path);
    if (!f.good()) {
        out_error = "failed to open";
        return false;
    }
    SpriteDef def{};
    // Defaults
    def.loop = false;
    def.fps = 0.0f;
    def.pivot_px_x = -1;
    def.pivot_px_y = -1;
    def.world_offset_x = 0.0f;
    def.world_offset_y = 0.0f;

    std::string line;
    bool in_frames = false;
    std::vector<SpriteFrame> frames_tmp;
    while (std::getline(f, line)) {
        std::string t = trim(line);
        if (t.empty() || t[0] == '#')
            continue;
        if (!in_frames && t.rfind("frames", 0) == 0) {
            auto eq = t.find('=');
            if (eq == std::string::npos)
                continue;
            std::string rhs = trim(t.substr(eq + 1));
            if (rhs.size() && rhs[0] == '[' && rhs.back() == ']') {
                // single-line frames array
                std::string inner = trim(rhs.substr(1, rhs.size() - 2));
                auto items = split_csv(inner);
                for (const auto& it : items) {
                    std::string s = trim(it);
                    if (s.size() < 2 || s.front() != '[' || s.back() != ']')
                        continue;
                    auto parts = split_csv(trim(s.substr(1, s.size() - 2)));
                    if (parts.size() < 4)
                        continue;
                    SpriteFrame fr{};
                    if (!parse_int(parts[0], fr.x))
                        continue;
                    if (!parse_int(parts[1], fr.y))
                        continue;
                    if (!parse_int(parts[2], fr.w))
                        continue;
                    if (!parse_int(parts[3], fr.h))
                        continue;
                    if (parts.size() >= 5) {
                        (void)parse_float(parts[4], fr.duration_sec);
                    }
                    frames_tmp.push_back(fr);
                }
            } else if (rhs == "[") {
                in_frames = true;
            }
            continue;
        }
        if (in_frames) {
            if (t == "]") {
                in_frames = false;
                continue;
            }
            // Expect lines like: [x, y, w, h] or with optional duration
            std::string s = t;
            if (!s.empty() && s.back() == ',')
                s.pop_back();
            if (s.size() < 2 || s.front() != '[' || s.back() != ']')
                continue;
            auto parts = split_csv(trim(s.substr(1, s.size() - 2)));
            if (parts.size() < 4)
                continue;
            SpriteFrame fr{};
            if (!parse_int(parts[0], fr.x))
                continue;
            if (!parse_int(parts[1], fr.y))
                continue;
            if (!parse_int(parts[2], fr.w))
                continue;
            if (!parse_int(parts[3], fr.h))
                continue;
            if (parts.size() >= 5) {
                (void)parse_float(parts[4], fr.duration_sec);
            }
            frames_tmp.push_back(fr);
            continue;
        }

        // key = value
        auto eq = t.find('=');
        if (eq == std::string::npos)
            continue;
        std::string key = trim(t.substr(0, eq));
        std::string val = trim(t.substr(eq + 1));
        // strip quotes for strings
        if (!val.empty() && val.front() == '"' && val.back() == '"') {
            val = val.substr(1, val.size() - 2);
        }
        if (key == "name") {
            def.name = val;
        } else if (key == "image") {
            def.image_path = val;
        } else if (key == "fps") {
            (void)parse_float(val, def.fps);
        } else if (key == "loop") {
            (void)parse_bool(val, def.loop);
        } else if (key == "pivot_px") {
            int a = -1, b = -1;
            if (parse_int_pair(val, a, b)) {
                def.pivot_px_x = a;
                def.pivot_px_y = b;
            }
        } else if (key == "world_offset") {
            std::string s = val;
            if (s.size() < 2 || s.front() != '[' || s.back() != ']') { /* ignore */
            } else {
                std::string inner = trim(s.substr(1, s.size() - 2));
                auto parts = split_csv(inner);
                if (parts.size() == 2) {
                    (void)parse_float(parts[0], def.world_offset_x);
                    (void)parse_float(parts[1], def.world_offset_y);
                }
            }
        } else if (key == "grid") {
            // grid = { frame_w=16, frame_h=16, cols=4, rows=1, origin_x=0, origin_y=0 }
            if (val.size() < 2 || val.front() != '{' || val.back() != '}')
                continue;
            std::string inner = trim(val.substr(1, val.size() - 2));
            auto kvs = split_csv(inner);
            int fw = 0, fh = 0, cols = 0, rows = 0, ox = 0, oy = 0;
            for (const auto& kvs_item : kvs) {
                auto eq2 = kvs_item.find('=');
                if (eq2 == std::string::npos)
                    continue;
                std::string k = trim(kvs_item.substr(0, eq2));
                std::string v = trim(kvs_item.substr(eq2 + 1));
                (void)parse_int(v, fw);
                (void)parse_int(v, fh); // to avoid -Wunused warnings when not matched
                if (k == "frame_w")
                    (void)parse_int(v, fw);
                else if (k == "frame_h")
                    (void)parse_int(v, fh);
                else if (k == "cols")
                    (void)parse_int(v, cols);
                else if (k == "rows")
                    (void)parse_int(v, rows);
                else if (k == "origin_x")
                    (void)parse_int(v, ox);
                else if (k == "origin_y")
                    (void)parse_int(v, oy);
            }
            if (fw > 0 && fh > 0 && cols > 0 && rows > 0) {
                frames_tmp.clear();
                frames_tmp.reserve(static_cast<size_t>(cols * rows));
                for (int r = 0; r < rows; ++r) {
                    for (int c = 0; c < cols; ++c) {
                        SpriteFrame fr{};
                        fr.x = ox + c * fw;
                        fr.y = oy + r * fh;
                        fr.w = fw;
                        fr.h = fh;
                        fr.duration_sec = 0.0f;
                        frames_tmp.push_back(fr);
                    }
                }
            }
        }
    }

    if (def.name.empty()) {
        // derive from filename stem
        def.name = fs::path(path).stem().string();
    }
    if (def.image_path.empty()) {
        out_error = "image not specified";
        return false;
    }
    if (!frames_tmp.empty())
        def.frames = std::move(frames_tmp);
    if (def.frames.empty()) {
        // Default to whole image placeholder
        SpriteFrame fr{};
        def.frames.push_back(fr);
    }
    out_def = std::move(def);
    return true;
}
