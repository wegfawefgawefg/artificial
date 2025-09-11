#include "app.hpp"

#include <cstdio>

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

