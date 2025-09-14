#include "graphics.hpp"
#include "globals.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include <algorithm>
#include <cctype>

bool init_font(const char* fonts_dir, int pt_size) {
    if (gg->ui_font)
        return true;
    if (!TTF_WasInit()) {
        if (TTF_Init() != 0) {
            std::fprintf(stderr, "TTF_Init failed: %s\n", TTF_GetError());
            return false;
        }
    }
    std::string font_path;
    std::error_code ec;
    std::filesystem::path fdir = std::filesystem::path(fonts_dir);
    if (std::filesystem::exists(fdir, ec) && std::filesystem::is_directory(fdir, ec)) {
        for (auto const& de : std::filesystem::directory_iterator(fdir, ec)) {
            if (ec) { ec.clear(); continue; }
            if (!de.is_regular_file()) continue;
            auto p = de.path();
            auto ext = p.extension().string();
            for (auto& c : ext) c = (char)std::tolower((unsigned char)c);
            if (ext == ".ttf") { font_path = p.string(); break; }
        }
    }
    if (!font_path.empty()) {
        gg->ui_font = TTF_OpenFont(font_path.c_str(), pt_size);
        if (!gg->ui_font) {
            std::fprintf(stderr, "TTF_OpenFont failed: %s\n", TTF_GetError());
            return false;
        }
        return true;
    } else {
        std::fprintf(stderr, "No .ttf found in %s. Numeric countdown will be hidden.\n", fonts_dir);
        return false;
    }
}

bool try_init_video_with_driver(const char* driver) {
    if (driver) {
        setenv("SDL_VIDEODRIVER", driver, 1);
    }
    if (SDL_Init(SDL_INIT_VIDEO) == 0)
        return true;
    const char* err = SDL_GetError();
    std::fprintf(stderr, "SDL_Init failed (driver=%s): %s\n", driver ? driver : "auto",
                 (err && *err) ? err : "(no error text)");
    return false;
}

bool init_graphics(bool headless) {
    gg = new Graphics{};

    const char* title = "artificial";
    glm::ivec2 window_dims = {1280, 720};

    gg->window = nullptr;
    gg->renderer = nullptr;
    gg->window_dims = {static_cast<unsigned int>(window_dims.x), static_cast<unsigned int>(window_dims.y)};
    gg->dims = gg->window_dims;

    // Initialize SDL video with driver selection
    const char* env_display = std::getenv("DISPLAY");
    const char* env_wayland = std::getenv("WAYLAND_DISPLAY");
    const char* env_sdl_driver = std::getenv("SDL_VIDEODRIVER");

    if (headless) {
        if (!try_init_video_with_driver("dummy"))
            return false;
        // No window/renderer in headless
        return true;
    }

    // Ignore accidental dummy driver in non-headless mode
    if (env_sdl_driver && std::strcmp(env_sdl_driver, "dummy") == 0) {
        unsetenv("SDL_VIDEODRIVER");
        env_sdl_driver = nullptr;
    }

    bool initialized = false;
    if (env_sdl_driver && *env_sdl_driver)
        initialized = try_init_video_with_driver(env_sdl_driver);
    else
        initialized = try_init_video_with_driver(nullptr); // auto-pick

    if (!initialized && env_display && *env_display)
        initialized = try_init_video_with_driver("x11");
    if (!initialized && env_wayland && *env_wayland)
        initialized = try_init_video_with_driver("wayland");
    if (!initialized)
        return false;

    Uint32 win_flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALWAYS_ON_TOP | SDL_WINDOW_UTILITY;
    gg->window = SDL_CreateWindow(title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                  window_dims.x, window_dims.y, win_flags);
    if (!gg->window) {
        const char* err = SDL_GetError();
        std::fprintf(stderr, "SDL_CreateWindow failed: %s\n", (err && *err) ? err : "(no error text)");
        return false;
    }

    SDL_SetWindowAlwaysOnTop(gg->window, SDL_TRUE);

    gg->renderer = SDL_CreateRenderer(gg->window, -1, SDL_RENDERER_ACCELERATED);
    if (!gg->renderer) {
        std::fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
        gg->renderer = SDL_CreateRenderer(gg->window, -1, 0); // software fallback
    }
    if (!gg->renderer)
        return false;

    const char* active_driver = SDL_GetCurrentVideoDriver();
    std::printf("SDL video driver: %s\n", active_driver ? active_driver : "(none)");

    // Initialize default UI font (optional)
    (void)init_font();
    return true;
}

void cleanup_graphics() {
    if (gg->ui_font) { TTF_CloseFont(gg->ui_font); gg->ui_font = nullptr; }
    if (gg->renderer) {
        SDL_DestroyRenderer(gg->renderer);
        gg->renderer = nullptr;
    }
    if (gg->window) {
        SDL_DestroyWindow(gg->window);
        gg->window = nullptr;
    }
    if (TTF_WasInit()) TTF_Quit();
}


// ---- Registry ----

void build_sprite_name_id_mapping(const std::vector<std::string>& names) {
    gg->sprite_name_to_id.clear();
    gg->sprite_id_to_name.clear();
    gg->sprite_id_to_name.reserve(names.size());
    for (const auto& n : names) {
        int id = static_cast<int>(gg->sprite_id_to_name.size());
        gg->sprite_name_to_id.emplace(n, id);
        gg->sprite_id_to_name.push_back(n);
    }
}

int add_or_get_sprite_id(const std::string& name) {
    auto it = gg->sprite_name_to_id.find(name);
    if (it != gg->sprite_name_to_id.end()) return it->second;
    int id = static_cast<int>(gg->sprite_id_to_name.size());
    gg->sprite_name_to_id.emplace(name, id);
    gg->sprite_id_to_name.push_back(name);
    return id;
}

int try_get_sprite_id(const std::string& name) {
    auto it = gg->sprite_name_to_id.find(name);
    return (it == gg->sprite_name_to_id.end()) ? -1 : it->second;
}

// ---- Sprite definitions ----

void rebuild_sprite_mapping(const std::vector<SpriteDef>& new_defs) {

    auto& name_to_id = gg->sprite_name_to_id;
    auto& id_to_name = gg->sprite_id_to_name;
    auto& defs_by_id = gg->sprite_defs_by_id;

    bool only_additions = true;
    if (!name_to_id.empty()) {
        std::unordered_map<std::string, int> new_names;
        new_names.reserve(new_defs.size());
        for (const auto& d : new_defs) new_names.emplace(d.name, 1);
        for (const auto& kv : name_to_id) {
            if (new_names.find(kv.first) == new_names.end()) { only_additions = false; break; }
        }
    }

    std::unordered_map<std::string, int> new_name_to_id;
    std::vector<std::string> new_id_to_name;
    std::vector<SpriteDef> new_defs_by_id;
    new_name_to_id.reserve(new_defs.size());
    new_id_to_name.reserve(new_defs.size());

    if (!name_to_id.empty() && only_additions) {
        new_defs_by_id = std::vector<SpriteDef>(defs_by_id.size());
        new_id_to_name = id_to_name;
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
        for (const auto& d : new_defs) {
            if (new_name_to_id.find(d.name) != new_name_to_id.end()) continue;
            int id = static_cast<int>(new_defs_by_id.size());
            new_defs_by_id.push_back(d);
            new_id_to_name.push_back(d.name);
            new_name_to_id.emplace(d.name, id);
        }
    } else {
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

const SpriteDef* get_sprite_def_by_id(int id) {
    if (id < 0) return nullptr;
    size_t idx = static_cast<size_t>(id);
    if (idx >= gg->sprite_defs_by_id.size()) return nullptr;
    return &gg->sprite_defs_by_id[idx];
}

const SpriteDef* try_get_sprite_def(const std::string& name) {
    int id = try_get_sprite_id(name);
    return get_sprite_def_by_id(id);
}

// ---- Textures ----

void clear_textures() {
    for (auto& kv : gg->textures_by_id) {
        if (kv.second) SDL_DestroyTexture(kv.second);
    }
    gg->textures_by_id.clear();
}

/// Runs through the sprite defs from the last mod scan and loads the textures.
/// Heavy. Dont run often.
bool load_all_textures_in_sprite_lookup() {
    if (!gg->renderer) return false;
    for (int id = 0; id < static_cast<int>(gg->sprite_defs_by_id.size()); ++id) {
        const auto* def = get_sprite_def_by_id(id);
        if (!def) continue;
        if (def->image_path.empty()) continue;
        SDL_Texture* tex = IMG_LoadTexture(gg->renderer, def->image_path.c_str());
        if (!tex) {
            std::fprintf(stderr, "IMG_LoadTexture failed for %s: %s\n", def->image_path.c_str(),
                         IMG_GetError());
            continue;
        }
        gg->textures_by_id[id] = tex;
    }
    return true;
}

SDL_Texture* get_texture(int sprite_id) {
    auto it = gg->textures_by_id.find(sprite_id);
    return (it == gg->textures_by_id.end()) ? nullptr : it->second;
}

