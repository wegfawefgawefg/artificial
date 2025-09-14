#include "mods.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <fstream>
#include <system_error>

namespace fs = std::filesystem;

static std::string trim(const std::string& s) {
    size_t a = 0, b = s.size();
    while (a < b && std::isspace(static_cast<unsigned char>(s[a])))
        ++a;
    while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1])))
        --b;
    return s.substr(a, b - a);
}

static bool starts_with(const std::string& s, const char* pfx) {
    return s.rfind(pfx, 0) == 0;
}

void ModManager::track_tree(const std::string& path) {
    std::error_code ec;
    fs::path p(path);
    if (!fs::exists(p, ec))
        return;
    if (fs::is_regular_file(p, ec)) {
        tracked_files[p.string()] = fs::last_write_time(p, ec);
        return;
    }
    if (!fs::is_directory(p, ec))
        return;
    for (auto const& entry : fs::recursive_directory_iterator(p, ec)) {
        if (ec) {
            ec.clear();
            continue;
        }
        if (entry.is_regular_file()) {
            auto sp = entry.path().string();
            tracked_files[sp] = fs::last_write_time(entry.path(), ec);
        }
    }
}

bool ModManager::check_changes(std::vector<std::string>& changed_assets,
                                std::vector<std::string>& changed_scripts) {
    bool any = false;
    std::error_code ec;
    // Re-scan tracked files and detect new/removed/modified
    std::unordered_map<std::string, fs::file_time_type> current;
    current.reserve(tracked_files.size());
    for (auto const& m : mods) {
        // info.toml
        fs::path ip = fs::path(m.path) / "info.toml";
        if (fs::exists(ip, ec))
            current[ip.string()] = fs::last_write_time(ip, ec);
        // graphics
        fs::path gdir = fs::path(m.path) / "graphics";
        if (fs::exists(gdir, ec) && fs::is_directory(gdir, ec)) {
            for (auto const& e : fs::recursive_directory_iterator(gdir, ec)) {
                if (ec) {
                    ec.clear();
                    continue;
                }
                if (e.is_regular_file())
                    current[e.path().string()] = fs::last_write_time(e.path(), ec);
            }
        }
        // scripts
        fs::path sdir = fs::path(m.path) / "scripts";
        if (fs::exists(sdir, ec) && fs::is_directory(sdir, ec)) {
            for (auto const& e : fs::recursive_directory_iterator(sdir, ec)) {
                if (ec) {
                    ec.clear();
                    continue;
                }
                if (e.is_regular_file())
                    current[e.path().string()] = fs::last_write_time(e.path(), ec);
            }
        }
    }

    // Compare with previous snapshot
    for (auto const& [path, ts] : current) {
        auto it = tracked_files.find(path);
        if (it == tracked_files.end()) {
            any = true;
            if (path.find("/graphics/") != std::string::npos)
                changed_assets.push_back(path);
            else if (path.find("/scripts/") != std::string::npos)
                changed_scripts.push_back(path);
            else if (path.rfind("info.toml") != std::string::npos) { /* ignore for now */
            }
        } else if (ts != it->second) {
            any = true;
            if (path.find("/graphics/") != std::string::npos)
                changed_assets.push_back(path);
            else if (path.find("/scripts/") != std::string::npos)
                changed_scripts.push_back(path);
            else if (path.rfind("info.toml") != std::string::npos) { /* ignore for now */
            }
        }
    }
    // Detect deletions
    for (auto const& [path, _] : tracked_files) {
        if (current.find(path) == current.end()) {
            any = true;
            if (path.find("/graphics/") != std::string::npos)
                changed_assets.push_back(path);
            else if (path.find("/scripts/") != std::string::npos)
                changed_scripts.push_back(path);
        }
    }
    // Update snapshot
    tracked_files = std::move(current);
    return any;
}

ModInfo ModManager::parse_info(const std::string& mod_path) {
    ModInfo info{};
    info.path = mod_path;
    std::ifstream f(mod_path + "/info.toml");
    if (!f.good()) {
        // derive name from folder leaf
        info.name = fs::path(mod_path).filename().string();
        info.version = "0.0.0";
        return info;
    }
    std::string line;
    while (std::getline(f, line)) {
        std::string t = trim(line);
        if (t.empty() || t[0] == '#')
            continue;
        if (starts_with(t, "name")) {
            auto pos = t.find('=');
            if (pos != std::string::npos) {
                std::string v = trim(t.substr(pos + 1));
                if (!v.empty() && v.front() == '"' && v.back() == '"')
                    v = v.substr(1, v.size() - 2);
                info.name = v;
            }
        } else if (starts_with(t, "version")) {
            auto pos = t.find('=');
            if (pos != std::string::npos) {
                std::string v = trim(t.substr(pos + 1));
                if (!v.empty() && v.front() == '"' && v.back() == '"')
                    v = v.substr(1, v.size() - 2);
                info.version = v;
            }
        } else if (starts_with(t, "deps")) {
            auto pos = t.find('=');
            if (pos != std::string::npos) {
                std::string v = trim(t.substr(pos + 1));
                // parse ["a","b"] minimally
                if (!v.empty() && v.front() == '[' && v.back() == ']') {
                    std::string inner = v.substr(1, v.size() - 2);
                    size_t i = 0;
                    while (i < inner.size()) {
                        while (i < inner.size() &&
                               std::isspace(static_cast<unsigned char>(inner[i])))
                            ++i;
                        if (i < inner.size() && inner[i] == '"') {
                            ++i;
                            size_t j = i;
                            while (j < inner.size() && inner[j] != '"')
                                ++j;
                            if (j < inner.size()) {
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