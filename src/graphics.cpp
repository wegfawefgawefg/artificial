#include "graphics.hpp"
#include "app.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>

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

    return true;
}

void shutdown_graphics(Graphics& gfx) {
    if (gfx.renderer) {
        SDL_DestroyRenderer(gfx.renderer);
        gfx.renderer = nullptr;
    }
    if (gfx.window) {
        SDL_DestroyWindow(gfx.window);
        gfx.window = nullptr;
    }
}
