#include "config.hpp"
#include "graphics.hpp"
#include "runtime_settings.hpp"
#include "sim.hpp"
#include "globals.hpp"
#include "luamgr.hpp"
#include "mods.hpp"
#include "projectiles.hpp"
#include "settings.hpp"
#include "audio.hpp"
#include "sprites.hpp"
#include "state.hpp"
#include "room.hpp"
#include "step.hpp"
#include "render.hpp"

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <glm/glm.hpp>
#include <random>
#include <string>
#include <unordered_set>
#include <vector>

int main(int argc, char** argv) {
    // Lightweight CLI args for non-interactive testing
    bool arg_headless = false;
    long arg_frames = -1; // <0 => unlimited
    for (int i = 1; i < argc; ++i) {
        std::string a(argv[i]);
        if (a == "--headless")
            arg_headless = true;
        else if (a.rfind("--frames=", 0) == 0) {
            std::string v = a.substr(9);
            try {
                arg_frames = std::stol(v);
            } catch (...) {
                arg_frames = -1;
            }
        }
    }

    if (
        !init_graphics(arg_headless)
    ) {
        SDL_Quit();
        return 1;
    }

    if (!init_state()) {
        SDL_Quit();
        return 1;
    }

    // Audio (SDL_mixer)
    if (!init_audio()) {
        std::fprintf(stderr, "[audio] SDL_mixer init failed: %s\n", Mix_GetError());
    }

    // Mods and assets
    if (!init_mods_manager()) {
        SDL_Quit();
        return 1;
    }
    discover_mods();
    scan_mods_for_sprite_defs();
    if (!arg_headless) {
        load_all_textures_in_sprite_lookup();
        load_mod_sounds();
    }

    auto lm = LuaManager{};
    luam = &lm;
    if (!luam->init()) {
        std::fprintf(stderr, "Lua 5.4 not available. Install lua5.4. Exiting.\n");
        return 1;
    }
    luam->load_mods();

    if (!load_input_bindings_from_ini("config/input.ini")) {
        return 1;
    }

    generate_room();

    // FPS counter state
    Uint64 perf_freq = SDL_GetPerformanceFrequency();
    Uint64 t_last = SDL_GetPerformanceCounter();
    float accum_sec = 0.0f;
    int frame_counter = 0;
    int last_fps = 0;
    
    std::string title_buf;
    
    // Main Loop
    while (ss->running) {
        // update dt
        Uint64 t_now = SDL_GetPerformanceCounter();
        float dt = static_cast<float>(t_now - t_last) / static_cast<float>(perf_freq);
        ss->dt = dt;
        t_last = t_now;
      

        // non game inputs event pump
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_WINDOWEVENT && ev.window.event == SDL_WINDOWEVENT_CLOSE)
                ss->running = false;
            process_event(ev);
            if (ev.type == SDL_QUIT)
                ss->running = false;
        }
        
        poll_fs_mods_hot_reload();
        
        collect_inputs();
        process_inputs();

        step();

        // Render a full frame via renderer module (windowed mode only)
        if (!arg_headless)
            render();

        // FPS calculation using high-resolution timer
        accum_sec += dt;
        frame_counter += 1;
        if (accum_sec >= 1.0f) {
            last_fps = frame_counter;
            frame_counter = 0;
            accum_sec -= 1.0f;
            title_buf.clear();
            title_buf.reserve(64);
            title_buf = "artificial - FPS: ";
            // Convert FPS to string without iostreams to keep it light
            char tmp[32];
            std::snprintf(tmp, sizeof(tmp), "%d", last_fps);
            title_buf += tmp;
            if (!arg_headless)
                SDL_SetWindowTitle(gg->window, title_buf.c_str());
        }

        // Auto-exit after a fixed number of frames if requested.
        if (arg_frames >= 0) {
            if (--arg_frames <= 0)
                ss->running = false;
        }
    }

    cleanup_audio();
    cleanup_graphics();
    // TODO probably theres more cleanup to do that isnt done
    SDL_Quit();
    return 0;
}
