#include "graphics.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>

bool init_font(Graphics& gfx, const char* fonts_dir, int pt_size) {
    if (gfx.ui_font)
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
        gfx.ui_font = TTF_OpenFont(font_path.c_str(), pt_size);
        if (!gfx.ui_font) {
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

bool init_graphics(Graphics& gfx, bool headless, const char* title, int width, int height) {
    gfx.window = nullptr;
    gfx.renderer = nullptr;
    gfx.window_dims = {static_cast<unsigned int>(width), static_cast<unsigned int>(height)};
    gfx.dims = gfx.window_dims;

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
    gfx.window = SDL_CreateWindow(title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                  width, height, win_flags);
    if (!gfx.window) {
        const char* err = SDL_GetError();
        std::fprintf(stderr, "SDL_CreateWindow failed: %s\n", (err && *err) ? err : "(no error text)");
        return false;
    }

    SDL_SetWindowAlwaysOnTop(gfx.window, SDL_TRUE);

    gfx.renderer = SDL_CreateRenderer(gfx.window, -1, SDL_RENDERER_ACCELERATED);
    if (!gfx.renderer) {
        std::fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
        gfx.renderer = SDL_CreateRenderer(gfx.window, -1, 0); // software fallback
    }
    if (!gfx.renderer)
        return false;

    const char* active_driver = SDL_GetCurrentVideoDriver();
    std::printf("SDL video driver: %s\n", active_driver ? active_driver : "(none)");

    // Initialize default UI font (optional)
    (void)init_font(gfx);
    return true;
}

void shutdown_graphics(Graphics& gfx) {
    if (gfx.ui_font) { TTF_CloseFont(gfx.ui_font); gfx.ui_font = nullptr; }
    if (gfx.renderer) {
        SDL_DestroyRenderer(gfx.renderer);
        gfx.renderer = nullptr;
    }
    if (gfx.window) {
        SDL_DestroyWindow(gfx.window);
        gfx.window = nullptr;
    }
    if (TTF_WasInit()) TTF_Quit();
}
