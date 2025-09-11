// Minimal SDL2 window + GLM usage
#include "config.hpp"
#include "graphics.hpp"
#include "app.hpp"
#include "render.hpp"
#include "sim.hpp"
#include "globals.hpp"
#include "input_system.hpp"
#include "luamgr.hpp"
#include "mods.hpp"
#include "projectiles.hpp"
#include "settings.hpp"
#include "sound.hpp"
#include "sprites.hpp"
#include "state.hpp"
#include "tex.hpp"

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

// Helpers
struct Projectiles; // fwd
#include "room.hpp"
// render helpers moved to render.cpp
// defined in globals.cpp
// LuaManager* g_lua_mgr;
// SpriteIdRegistry* g_sprite_ids;
// moved to room.hpp/room.cpp

// moved to app.cpp

int main(int argc, char** argv) {
    // Lightweight CLI args for non-interactive testing
    bool arg_scan_sprites = false;
    bool arg_headless = false;
    long arg_frames = -1; // <0 => unlimited
    for (int i = 1; i < argc; ++i) {
        std::string a(argv[i]);
        if (a == "--scan-sprites")
            arg_scan_sprites = true;
        else if (a == "--headless")
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

    // Non-SDL path: only scan and build sprite data, then exit.
    if (arg_scan_sprites) {
        ModsManager mods{"mods"};
        mods.discover_mods();
        SpriteIdRegistry sprites{};
        mods.build_sprite_registry(sprites);
        SpriteStore sprite_store{};
        mods.build_sprite_store(sprite_store);
        std::printf("scan-sprites: %d names, %d defs\n", sprites.size(), sprite_store.size());
        // Print a few names for visibility
        int shown = 0;
        for (const auto& n : sprite_store.names_by_id()) {
            if (shown++ >= 10)
                break;
            std::printf(" - %s\n", n.c_str());
        }
        return 0;
    }

    const char* env_display = std::getenv("DISPLAY");
    const char* env_wayland = std::getenv("WAYLAND_DISPLAY");
    const char* env_sdl_driver = std::getenv("SDL_VIDEODRIVER");

    if (arg_headless)
        env_sdl_driver = "dummy";

    if (!try_init_video_with_driver(env_sdl_driver)) {
        if (env_display && *env_display) {
            if (try_init_video_with_driver("x11"))
                goto init_ok;
        }
        if (env_wayland && *env_wayland) {
            if (try_init_video_with_driver("wayland"))
                goto init_ok;
        }
        // Last resort: dummy driver for headless environments
        if (try_init_video_with_driver("dummy")) {
            std::fprintf(stderr, "Using SDL dummy video driver; no window will be shown.\n");
        } else {
            return 1;
        }
    }
init_ok:

    const char* title = "gub";
    int width = 1280;
    int height = 720;

    const char* active_driver = SDL_GetCurrentVideoDriver();
    std::printf("SDL video driver: %s\n", active_driver ? active_driver : "(none)");

    Uint32 win_flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALWAYS_ON_TOP | SDL_WINDOW_UTILITY;
    SDL_Window* window = SDL_CreateWindow(title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                          width, height, win_flags);
    if (!window) {
        const char* err = SDL_GetError();
        std::fprintf(stderr, "SDL_CreateWindow failed: %s\n",
                     (err && *err) ? err : "(no error text)");
        SDL_Quit();
        return 1;
    }

    // Ensure always-on-top in case WM didn't honor the flag immediately
    SDL_SetWindowAlwaysOnTop(window, SDL_TRUE);

    // Create a renderer so we can paint a background instead of showing desktop
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) {
        std::fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
        renderer = SDL_CreateRenderer(window, -1, 0); // software fallback
    }

    // quick GLM sanity check
    glm::vec3 a(1.0f, 2.0f, 3.0f);
    glm::vec3 b(4.0f, 5.0f, 6.0f);
    float dot = glm::dot(a, b);
    std::printf("glm dot(a,b) = %f\n", static_cast<double>(dot));

    // Engine state/graphics
    State state{};
    Graphics gfx{};
    state.mode = ids::MODE_PLAYING;

    // Text rendering setup (SDL_ttf)
    TTF_Font* ui_font = nullptr;
    if (TTF_Init() == 0) {
        // Pick first .ttf in fonts/
        std::string font_path;
        std::error_code ec;
        std::filesystem::path fdir = std::filesystem::path("fonts");
        if (std::filesystem::exists(fdir, ec) && std::filesystem::is_directory(fdir, ec)) {
            for (auto const& de : std::filesystem::directory_iterator(fdir, ec)) {
                if (ec) {
                    ec.clear();
                    continue;
                }
                if (!de.is_regular_file())
                    continue;
                auto p = de.path();
                auto ext = p.extension().string();
                for (auto& c : ext)
                    c = (char)std::tolower((unsigned char)c);
                if (ext == ".ttf") {
                    font_path = p.string();
                    break;
                }
            }
        }
        if (!font_path.empty()) {
            ui_font = TTF_OpenFont(font_path.c_str(), 20);
            if (!ui_font) {
                std::fprintf(stderr, "TTF_OpenFont failed: %s\n", TTF_GetError());
            }
        } else {
            std::fprintf(stderr, "No .ttf found in fonts/. Numeric countdown will be hidden.\n");
        }
    } else {
        std::fprintf(stderr, "TTF_Init failed: %s\n", TTF_GetError());
    }

    // Audio (SDL_mixer)
    SoundStore sounds;
    if (!sounds.init()) {
        std::fprintf(stderr, "[audio] SDL_mixer init failed: %s\n", Mix_GetError());
    }

    // Mods and sprite registry/store
    ModsManager mods{"mods"};
    mods.discover_mods();
    SpriteIdRegistry sprites{};
    mods.build_sprite_registry(sprites);
    SpriteStore sprite_store{};
    mods.build_sprite_store(sprite_store);
    // Resolve sprite IDs for known entities/items later
    g_sprite_ids = &sprites;
    // Load textures for sprites
    TextureStore textures;
    textures.load_all(renderer, sprite_store);
    // Load sounds from mods/*/sounds/*.wav|*.ogg with key "mod:stem"
    {
        std::error_code ec;
        std::filesystem::path mroot = std::filesystem::path("mods");
        if (std::filesystem::exists(mroot, ec) && std::filesystem::is_directory(mroot, ec)) {
            for (auto const& mod : std::filesystem::directory_iterator(mroot, ec)) {
                if (ec) {
                    ec.clear();
                    continue;
                }
                if (!mod.is_directory())
                    continue;
                std::string modname = mod.path().filename().string();
                auto sp = mod.path() / "sounds";
                if (std::filesystem::exists(sp, ec) && std::filesystem::is_directory(sp, ec)) {
                    for (auto const& f : std::filesystem::directory_iterator(sp, ec)) {
                        if (ec) {
                            ec.clear();
                            continue;
                        }
                        if (!f.is_regular_file())
                            continue;
                        auto p = f.path();
                        std::string ext = p.extension().string();
                        for (auto& c : ext)
                            c = (char)std::tolower((unsigned char)c);
                        if (ext == ".wav" || ext == ".ogg") {
                            std::string stem = p.stem().string();
                            std::string key = modname + ":" + stem;
                            (void)sounds.load_file(key, p.string());
                        }
                    }
                }
            }
        }
    }

    // Lua content (required at runtime)
    LuaManager lua;
    if (!lua.available() || !lua.init()) {
        std::fprintf(stderr, "Lua 5.4 not available. Install lua5.4. Exiting.\n");
        return 1;
    }
    lua.load_mods("mods");
    g_lua_mgr = &lua;

    // Prepare projectiles then generate initial room
    Projectiles projectiles{};
    generate_room(state, projectiles, renderer, gfx);

    bool running = true;
    InputBindings binds{};
    if (auto loaded = load_input_bindings_from_ini("config/input.ini")) {
        binds = *loaded;
    }
    // projectiles already declared above
    state.gun_cooldown = 0.0f;
    InputContext ictx{};
    // FPS counter state
    Uint64 perf_freq = SDL_GetPerformanceFrequency();
    Uint64 t_last = SDL_GetPerformanceCounter();
    double accum_sec = 0.0;
    int frame_counter = 0;
    int last_fps = 0;
    std::string title_buf;
    while (running && state.running) {
        SDL_Event ev;
        bool request_quit = false;
        ictx.wheel_delta = 0.0f;
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_WINDOWEVENT && ev.window.event == SDL_WINDOWEVENT_CLOSE)
                request_quit = true;
            process_events(ev, ictx, state, request_quit);
            if (ev.type == SDL_QUIT)
                request_quit = true;
        }

        Uint64 t_now = SDL_GetPerformanceCounter();
        double dt_sec = static_cast<double>(t_now - t_last) / static_cast<double>(perf_freq);
        t_last = t_now;

        build_inputs(binds, ictx, state, gfx, static_cast<float>(dt_sec));
        // Age alerts and purge expired
        for (auto& al : state.alerts) {
            al.age += static_cast<float>(dt_sec);
        }
        state.alerts.erase(std::remove_if(state.alerts.begin(), state.alerts.end(),
                                          [](const State::Alert& al) {
                                              return al.purge_eof ||
                                                     (al.ttl >= 0.0f && al.age > al.ttl);
                                          }),
                           state.alerts.end());
        // Hot reload poll (assets + behaviors stubs)
        mods.poll_hot_reload(sprites, dt_sec);
        mods.poll_hot_reload(sprite_store, dt_sec);

        if (request_quit)
            running = false;

        // Update: fixed timestep simulation
        state.time_since_last_update += static_cast<float>(dt_sec);
        while (state.time_since_last_update > TIMESTEP) {
            state.time_since_last_update -= TIMESTEP;

            // Before-physics ticking (opt-in)
            sim_pre_physics_ticks(state);

            // Movement + physics: player controlled + NPC wander; keep inside non-block tiles
            sim_move_and_collide(state, gfx);

            // Shield regeneration after delay (3s no damage), global step, and active reload
            // progress/completion
            sim_shield_and_reload(state);

                    // Auto-pickup powerups on overlap
                    if (state.mode == ids::MODE_PLAYING && state.player_vid) {
                const Entity* p = state.entities.get(*state.player_vid);
                if (p) {
                    glm::vec2 ph = p->half_size();
                    float pl = p->pos.x - ph.x, pr = p->pos.x + ph.x;
                    float pt = p->pos.y - ph.y, pb = p->pos.y + ph.y;
                    for (auto& pu : state.pickups.data())
                        if (pu.active) {
                            float gl = pu.pos.x - 0.125f, gr = pu.pos.x + 0.125f;
                            float gt = pu.pos.y - 0.125f, gb = pu.pos.y + 0.125f;
                            bool overlap = !(pr <= gl || pl >= gr || pb <= gt || pt >= gb);
                            if (overlap) {
                                state.alerts.push_back(
                                    {std::string("Picked up ") + pu.name, 0.0f, 2.0f, false});
                                pu.active = false;
                                // Metrics: count powerups picked by player
                                if (state.player_vid) {
                                    if (auto* pm = state.metrics_for(*state.player_vid))
                                        pm->powerups_picked += 1;
                                }
                            }
                        }
                }
            }

            // Item pickup with F key; ground guns equip or add to inventory as VIDs
            if (state.mode == ids::MODE_PLAYING && state.player_vid) {
                sim_handle_pickups(state, sounds);
                // Simple separation to avoid intersecting ground items/guns
                sim_ground_repulsion(state);
            }

            // Toggle drop mode on Q edge
            sim_toggle_drop_mode(state);

            // Number keys: either drop from slot (if in drop mode) or select/use/equip
            sim_inventory_number_row(state);

            // Accumulate time-in-stage metrics
            if (state.mode == ids::MODE_PLAYING) {
                state.metrics.time_in_stage += TIMESTEP;
            }
            // Decrement input lockout (prevents accidental actions after pages)
            state.input_lockout_timer = std::max(0.0f, state.input_lockout_timer - TIMESTEP);
            state.pickup_lockout = std::max(0.0f, state.pickup_lockout - TIMESTEP);

            // Exit countdown and mode transitions
            if (state.mode == ids::MODE_PLAYING) {
                const Entity* p =
                    (state.player_vid ? state.entities.get(*state.player_vid) : nullptr);
                if (p) {
                    // Player AABB
                    glm::vec2 half = p->half_size();
                    float left = p->pos.x - half.x;
                    float right = p->pos.x + half.x;
                    float top = p->pos.y - half.y;
                    float bottom = p->pos.y + half.y;
                    // Exit tile AABB [x,x+1]x[y,y+1]
                    float exl = static_cast<float>(state.exit_tile.x);
                    float exr = exl + 1.0f;
                    float ext = static_cast<float>(state.exit_tile.y);
                    float exb = ext + 1.0f;
                    bool overlaps = !(right <= exl || left >= exr || bottom <= ext || top >= exb);
                    if (overlaps) {
                        if (state.exit_countdown < 0.0f) {
                            state.exit_countdown = EXIT_COUNTDOWN_SECONDS;
                            state.alerts.push_back(
                                {"Exit reached: hold to leave", 0.0f, 2.0f, false});
                            std::printf("[room] Exit reached, starting %.1fs countdown...\n",
                                        (double)EXIT_COUNTDOWN_SECONDS);
                        }
                    } else {
                        // Reset countdown if player leaves tile
                        if (state.exit_countdown >= 0.0f) {
                            state.exit_countdown = -1.0f;
                            state.alerts.push_back({"Exit canceled", 0.0f, 1.5f, false});
                            std::printf("[room] Exit countdown canceled (left tile).\n");
                        }
                    }
                }
                if (state.exit_countdown >= 0.0f) {
                    state.exit_countdown -= TIMESTEP;
                    if (state.exit_countdown <= 0.0f) {
                        state.exit_countdown = -1.0f;
                        state.mode = ids::MODE_SCORE_REVIEW;
                        state.score_ready_timer = SCORE_REVIEW_INPUT_DELAY;
                        state.alerts.push_back({"Area complete", 0.0f, 2.5f, false});
                        std::printf("[room] Countdown complete. Entering score review.\n");
                        // Prepare review stats and animation
                        state.review_stats.clear();
                        state.review_revealed = 0;
                        state.review_next_stat_timer = 0.0f;
                        state.review_number_tick_timer = 0.0f;
                        auto add_header = [&](const std::string& s) {
                            state.review_stats.push_back(State::ReviewStat{s, 0.0, 0.0, true, true});
                        };
                        auto add_stat = [&](const std::string& s, double v) {
                            state.review_stats.push_back(State::ReviewStat{s, v, 0.0, false, false});
                        };
                        // Aggregate some totals across players
                        std::uint64_t total_shots_fired = 0, total_shots_hit = 0;
                        std::uint64_t total_enemies_slain = state.metrics.enemies_slain;
                        std::uint64_t total_powerups_picked = 0, total_items_picked = 0,
                                     total_guns_picked = 0, total_items_dropped = 0,
                                     total_guns_dropped = 0, total_damage_dealt = 0;
                        for (auto const& e : state.entities.data()) {
                            if (!e.active || e.type_ != ids::ET_PLAYER)
                                continue;
                            const auto* pm = state.metrics_for(e.vid);
                            if (!pm)
                                continue;
                            total_shots_fired += pm->shots_fired;
                            total_shots_hit += pm->shots_hit;
                            total_powerups_picked += pm->powerups_picked;
                            total_items_picked += pm->items_picked;
                            total_guns_picked += pm->guns_picked;
                            total_items_dropped += pm->items_dropped;
                            total_guns_dropped += pm->guns_dropped;
                            total_damage_dealt += pm->damage_dealt;
                        }
                        std::int64_t missed_powerups = (std::int64_t)state.metrics.powerups_spawned - (std::int64_t)total_powerups_picked;
                        std::int64_t missed_items = (std::int64_t)state.metrics.items_spawned - (std::int64_t)total_items_picked;
                        std::int64_t missed_guns = (std::int64_t)state.metrics.guns_spawned - (std::int64_t)total_guns_picked;
                        if (missed_powerups < 0) missed_powerups = 0;
                        if (missed_items < 0) missed_items = 0;
                        if (missed_guns < 0) missed_guns = 0;

                        // Global
                        add_stat("Time (s)", state.metrics.time_in_stage);
                        add_stat("Crates opened", (double)state.metrics.crates_opened);
                        add_stat("Enemies slain", (double)total_enemies_slain);
                        add_stat("Damage dealt", (double)total_damage_dealt);
                        add_stat("Shots fired (total)", (double)total_shots_fired);
                        add_stat("Shots hit (total)", (double)total_shots_hit);
                        double acc_total = total_shots_fired ? (100.0 * (double)total_shots_hit / (double)total_shots_fired) : 0.0;
                        add_stat("Accuracy total (%)", acc_total);
                        add_stat("Powerups picked (total)", (double)total_powerups_picked);
                        add_stat("Items picked (total)", (double)total_items_picked);
                        add_stat("Guns picked (total)", (double)total_guns_picked);
                        add_stat("Items dropped (total)", (double)total_items_dropped);
                        add_stat("Guns dropped (total)", (double)total_guns_dropped);
                        add_stat("Missed powerups", (double)missed_powerups);
                        add_stat("Missed items", (double)missed_items);
                        add_stat("Missed guns", (double)missed_guns);
                        // Per-player
                        int pidx = 1;
                        for (auto const& e : state.entities.data()) {
                            if (!e.active || e.type_ != ids::ET_PLAYER)
                                continue;
                            const auto* pm = state.metrics_for(e.vid);
                            if (!pm)
                                continue;
                            char hdr[32];
                            std::snprintf(hdr, sizeof(hdr), "Player %d", pidx++);
                            add_header(hdr);
                            add_stat("  Shots fired", (double)pm->shots_fired);
                            add_stat("  Shots hit", (double)pm->shots_hit);
                            double acc = pm->shots_fired ? (100.0 * (double)pm->shots_hit / (double)pm->shots_fired) : 0.0;
                            add_stat("  Accuracy (%)", acc);
                            add_stat("  Enemies slain", (double)pm->enemies_slain);
                            add_stat("  Dashes used", (double)pm->dashes_used);
                            add_stat("  Dash distance", (double)pm->dash_distance);
                            add_stat("  Powerups picked", (double)pm->powerups_picked);
                            add_stat("  Items picked", (double)pm->items_picked);
                            add_stat("  Guns picked", (double)pm->guns_picked);
                            add_stat("  Items dropped", (double)pm->items_dropped);
                            add_stat("  Guns dropped", (double)pm->guns_dropped);
                            add_stat("  Reloads", (double)pm->reloads);
                            add_stat("  AR success", (double)pm->active_reload_success);
                            add_stat("  AR failed", (double)pm->active_reload_fail);
                            add_stat("  Jams", (double)pm->jams);
                            add_stat("  Unjam mashes", (double)pm->unjam_mashes);
                            add_stat("  Damage dealt", (double)pm->damage_dealt);
                            add_stat("  Damage taken HP", (double)pm->damage_taken_hp);
                            add_stat("  Damage to shields", (double)pm->damage_taken_shield);
                            add_stat("  Plates gained", (double)pm->plates_gained);
                            add_stat("  Plates consumed", (double)pm->plates_consumed);
                        }
                    }
                }
            } else if (state.mode == ids::MODE_SCORE_REVIEW || state.mode == ids::MODE_NEXT_STAGE) {
                if (state.score_ready_timer > 0.0f)
                    state.score_ready_timer -= TIMESTEP;
            }

            // Camera follow: move towards mix of player and mouse world position
            if (state.player_vid) {
                const Entity* p = state.entities.get(*state.player_vid);
                if (p) {
                    int ww = width, wh = height;
                    SDL_GetRendererOutputSize(renderer, &ww, &wh);
                    float zx = gfx.play_cam.zoom;
                    float sx = static_cast<float>(state.mouse_inputs.pos.x);
                    float sy = static_cast<float>(state.mouse_inputs.pos.y);
                    // screen pixels to world units
                    glm::vec2 mouse_world = p->pos; // default fallback
                    float inv_scale = 1.0f / (TILE_SIZE * zx);
                    mouse_world.x =
                        gfx.play_cam.pos.x + (sx - static_cast<float>(ww) * 0.5f) * inv_scale;
                    mouse_world.y =
                        gfx.play_cam.pos.y + (sy - static_cast<float>(wh) * 0.5f) * inv_scale;

                    glm::vec2 target = p->pos;
                    if (state.camera_follow_enabled) {
                        target = p->pos + (mouse_world - p->pos) * CAMERA_FOLLOW_FACTOR;
                    }
                    gfx.play_cam.pos = target;
                }
            }

            // Reload handling (R key): active reload system
            if (state.mode == ids::MODE_PLAYING && state.player_vid) {
                auto* plm = state.entities.get_mut(*state.player_vid);
                if (plm && plm->equipped_gun_vid.has_value()) {
                    static bool prev_reload = false;
                    bool now_reload = state.playing_inputs.reload;
                    if (now_reload && !prev_reload) {
                        GunInstance* gim = state.guns.get(*plm->equipped_gun_vid);
                        if (gim) {
                            const GunDef* gd = nullptr;
                            if (g_lua_mgr) {
                                for (auto const& g : g_lua_mgr->guns())
                                    if (g.type == gim->def_type) {
                                        gd = &g;
                                        break;
                                    }
                            }
                            // Do not allow reloading while jammed
                            if (gim->jammed) {
                                state.alerts.push_back({"Gun jammed! Mash SPACE", 0.0f, 1.2f, false});
                                // Gentle feedback sound when trying to reload while jammed
                                sounds.play("base:ui_cant");
                            } else if (gd) {
                                if (gim->reloading) {
                                    // Active reload attempt
                                    float prog = gim->reload_progress;
                                    if (gim->reload_total_time > 0.0f)
                                        prog = gim->reload_progress; // already 0..1
                                    if (!gim->ar_consumed && prog >= gim->ar_window_start &&
                                        prog <= gim->ar_window_end) {
                                        // Success: instant reload
                                        int take = std::min(gd->mag, gim->ammo_reserve);
                                        gim->current_mag = take;
                                        gim->ammo_reserve -= take;
                                        gim->reloading = false;
                                        gim->reload_progress = 0.0f;
                                        // Clear any pending burst so reload completion doesn't resume firing
                                        gim->burst_remaining = 0;
                                        gim->burst_timer = 0.0f;
                                        state.alerts.push_back(
                                            {"Active Reload!", 0.0f, 1.2f, false});
                                        state.reticle_shake = std::max(state.reticle_shake, 6.0f);
                                        sounds.play("base:ui_super_confirm");
                                        // Metrics
                                        if (state.player_vid) {
                                            if (auto* pm = state.metrics_for(*state.player_vid))
                                                pm->active_reload_success += 1;
                                        }
                                        // Hooks: global, gun, items
                                        if (g_lua_mgr) {
                                            g_lua_mgr->call_on_active_reload(state, *plm);
                                            g_lua_mgr->call_gun_on_active_reload(gim->def_type,
                                                                                 state, *plm);
                                            for (const auto& entry : state.inventory.entries) {
                                                if (entry.kind == INV_ITEM) {
                                                    if (const ItemInstance* inst =
                                                            state.items.get(entry.vid)) {
                                                        g_lua_mgr->call_item_on_active_reload(
                                                            inst->def_type, state, *plm);
                                                    }
                                                }
                                            }
                                        }
                                    } else if (!gim->ar_consumed) {
                                        // Failure: lock out further attempts this reload
                                        gim->ar_consumed = true;
                                        gim->ar_failed_attempt = true;
                                        state.reload_bar_shake =
                                            std::max(state.reload_bar_shake, 6.0f);
                                        state.alerts.push_back(
                                            {"Active Reload Failed", 0.0f, 0.7f, false});
                                        // Metrics
                                        if (state.player_vid) {
                                            if (auto* pm = state.metrics_for(*state.player_vid))
                                                pm->active_reload_fail += 1;
                                        }
                                        if (g_lua_mgr) {
                                            g_lua_mgr->call_on_failed_active_reload(state, *plm);
                                            g_lua_mgr->call_gun_on_failed_active_reload(
                                                gim->def_type, state, *plm);
                                            for (const auto& entry : state.inventory.entries) {
                                                if (entry.kind == INV_ITEM) {
                                                    if (const ItemInstance* inst =
                                                            state.items.get(entry.vid)) {
                                                        g_lua_mgr->call_item_on_failed_active_reload(
                                                            inst->def_type, state, *plm);
                                                    }
                                                }
                                            }
                                        }
                                    } else if (gim->ar_consumed && gim->ar_failed_attempt) {
                                        // Already failed this reload; notify hook when trying again
                                        if (g_lua_mgr) {
                                            g_lua_mgr->call_on_tried_after_failed_ar(state, *plm);
                                            g_lua_mgr->call_gun_on_tried_after_failed_ar(
                                                gim->def_type, state, *plm);
                                            for (const auto& entry : state.inventory.entries) {
                                                if (entry.kind == INV_ITEM) {
                                                    if (const ItemInstance* inst =
                                                            state.items.get(entry.vid)) {
                                                        g_lua_mgr->call_item_on_tried_after_failed_ar(
                                                            inst->def_type, state, *plm);
                                                    }
                                                }
                                            }
                                        }
                                    }
                                } else if (gim->ammo_reserve > 0) {
                                    int dropped = gim->current_mag;
                                    if (dropped > 0)
                                        state.alerts.push_back({std::string("Dropped ") +
                                                                    std::to_string(dropped) +
                                                                    " bullets",
                                                                0.0f, 1.0f, false});
                                    gim->current_mag = 0;
                                    // Start active reload sequence
                                    gim->reloading = true;
                                    // Metrics: reloads
                                    if (state.player_vid) {
                                        if (auto* pm = state.metrics_for(*state.player_vid))
                                            pm->reloads += 1;
                                    }
                                    gim->reload_progress = 0.0f;
                                    gim->reload_eject_remaining = std::max(0.0f, gd->eject_time);
                                    gim->reload_total_time = std::max(0.1f, gd->reload_time);
                                    // Cancel any in-progress burst so it can't continue after reload
                                    gim->burst_remaining = 0;
                                    gim->burst_timer = 0.0f;
                                    // Cancel any in-progress burst so it can't continue after reload
                                    gim->burst_remaining = 0;
                                    gim->burst_timer = 0.0f;
                                    static thread_local std::mt19937 rng{std::random_device{}()};
                                    auto clamp01 = [](float v) {
                                        return std::max(0.0f, std::min(1.0f, v));
                                    };
                                    std::uniform_real_distribution<float> Upos(-gd->ar_pos_variance,
                                                                               gd->ar_pos_variance);
                                    std::uniform_real_distribution<float> Usize(
                                        -gd->ar_size_variance, gd->ar_size_variance);
                                    float size = std::clamp(gd->ar_size + Usize(rng), 0.02f, 0.9f);
                                    float center = clamp01(gd->ar_pos + Upos(rng));
                                    float start = clamp01(center - size * 0.5f);
                                    if (start + size > 1.0f)
                                        start = 1.0f - size;
                                    gim->ar_window_start = start;
                                    gim->ar_window_end = start + size;
                                    gim->ar_consumed = false;
                                    gim->ar_failed_attempt = false;
                                    sounds.play(gd->sound_reload.empty() ? "base:reload"
                                                                         : gd->sound_reload);
                                } else {
                                    state.alerts.push_back(
                                        {std::string("NO AMMO"), 0.0f, 1.5f, false});
                                }
                            }
                        }
                    }
                    prev_reload = now_reload;
                }
            }

            // spawn projectile on left mouse with small cooldown (uses gun rpm if equipped)
            state.gun_cooldown = std::max(0.0f, state.gun_cooldown - TIMESTEP);
            bool can_fire = (state.gun_cooldown == 0.0f);
            static bool prev_shoot = false;
            bool trig_held = (state.mode == ids::MODE_PLAYING) ? state.mouse_inputs.left : false;
            bool trig_edge = trig_held && !prev_shoot;
            // Update prev only during gameplay to avoid edge after pages
            if (state.mode == ids::MODE_PLAYING)
                prev_shoot = trig_held;
            bool fire_request = false;
            bool burst_step = false;
            int burst_count = 0;
            float burst_rpm = 0.0f;
            std::string fire_mode = "auto";
            if (state.mode == ids::MODE_PLAYING && state.player_vid) {
                auto* plm = state.entities.get_mut(*state.player_vid);
                if (plm && plm->equipped_gun_vid.has_value()) {
                    const GunInstance* giq = state.guns.get(*plm->equipped_gun_vid);
                    const GunDef* gdq = nullptr;
                    if (g_lua_mgr && giq) {
                        for (auto const& g : g_lua_mgr->guns())
                            if (g.type == giq->def_type) {
                                gdq = &g;
                                break;
                            }
                    }
                    if (gdq) {
                        fire_mode = gdq->fire_mode;
                        burst_count = gdq->burst_count;
                        burst_rpm = gdq->burst_rpm;
                    }
                    GunInstance* gimq = state.guns.get(*plm->equipped_gun_vid);
                    if (gimq) {
                        gimq->burst_timer = std::max(0.0f, gimq->burst_timer - TIMESTEP);
                        // Recoil spread decay (control = deg/sec)
                        if (gdq) {
                            gimq->spread_recoil_deg = std::max(0.0f, gimq->spread_recoil_deg - gdq->control * TIMESTEP);
                        }
                        if (fire_mode == "auto")
                            fire_request = trig_held;
                        else if (fire_mode == "single")
                            fire_request = trig_edge;
                        else if (fire_mode == "burst") {
                            if (trig_edge && gimq->burst_remaining == 0 && burst_count > 0) {
                                gimq->burst_remaining = burst_count;
                            }
                            if (gimq->burst_remaining > 0 && gimq->burst_timer == 0.0f) {
                                fire_request = true;
                                burst_step = true;
                            }
                        }
                    }
                } else {
                    fire_request = false;
                }
            } else {
                fire_request = trig_held;
            }
            if (state.mode == ids::MODE_PLAYING && state.input_lockout_timer == 0.0f && fire_request && can_fire) {
                // spawn from screen center towards mouse in world-space
                glm::vec2 p = state.player_vid ? state.entities.get(*state.player_vid)->pos
                                               : glm::vec2{(float)state.stage.get_width() / 2.0f,
                                                           (float)state.stage.get_height() / 2.0f};
                // convert mouse to world
                int ww = width, wh = height;
                SDL_GetRendererOutputSize(renderer, &ww, &wh);
                float inv_scale = 1.0f / (TILE_SIZE * gfx.play_cam.zoom);
                glm::vec2 m = {gfx.play_cam.pos.x + (static_cast<float>(state.mouse_inputs.pos.x) -
                                                     static_cast<float>(ww) * 0.5f) *
                                                        inv_scale,
                               gfx.play_cam.pos.y + (static_cast<float>(state.mouse_inputs.pos.y) -
                                                     static_cast<float>(wh) * 0.5f) *
                                                        inv_scale};
                glm::vec2 aim = glm::normalize(m - p);
                glm::vec2 dir = aim;
                if (glm::any(glm::isnan(dir)))
                    dir = {1.0f, 0.0f};
                // If gun equipped, respect ammo and rpm
                float rpm = 600.0f; // default
                bool fired = true;
                int proj_type = 0;
                float proj_speed = 20.0f;
                glm::vec2 proj_size{0.2f, 0.2f};
                int proj_steps = 2;
                int proj_sprite_id = -1;
                int ammo_type = 0;
                if (state.player_vid) {
                    auto* plm = state.entities.get_mut(*state.player_vid);
                    if (plm && plm->equipped_gun_vid.has_value()) {
                        const GunInstance* gi = state.guns.get(*plm->equipped_gun_vid);
                        const GunDef* gd = nullptr;
                        if (g_lua_mgr && gi) {
                            for (auto const& g : g_lua_mgr->guns())
                                if (g.type == gi->def_type) {
                                    gd = &g;
                                    break;
                                }
                        }
                        if (gd && gi) {
                            rpm = (gd->rpm > 0.0f) ? gd->rpm : rpm;
                            // projectile def lookup
                            if (g_lua_mgr && gd->projectile_type != 0) {
                                if (auto const* pd =
                                        g_lua_mgr->find_projectile(gd->projectile_type)) {
                                    proj_type = pd->type;
                                    proj_speed = pd->speed;
                                    proj_size = {pd->size_x, pd->size_y};
                                    proj_steps = pd->physics_steps;
                                    if (!pd->sprite.empty() &&
                                        pd->sprite.find(':') != std::string::npos && g_sprite_ids) {
                                        proj_sprite_id = g_sprite_ids->try_get(pd->sprite);
                                    }
                                }
                            }
                            // Ammo type selected on gun generation
                            ammo_type = gi->ammo_type;
                            // Apply ammo overrides
                            if (g_lua_mgr && ammo_type != 0) {
                                if (auto const* ad = g_lua_mgr->find_ammo(ammo_type)) {
                                    if (ad->speed > 0.0f) proj_speed = ad->speed;
                                    proj_size = {ad->size_x, ad->size_y};
                                    if (!ad->sprite.empty() && ad->sprite.find(':') != std::string::npos && g_sprite_ids) {
                                        int sid = g_sprite_ids->try_get(ad->sprite);
                                        if (sid >= 0) proj_sprite_id = sid;
                                    }
                                }
                            }
                            GunInstance* gim = state.guns.get(*plm->equipped_gun_vid);
                            if (gim->jammed || gim->reloading ||
                                gim->reload_eject_remaining > 0.0f) {
                                fired = false;
                            } else if (gim->current_mag > 0) {
                                gim->current_mag -= 1;
                            } else {
                                fired = false;
                            }
                            // Apply accuracy perturbation to direction
                            if (fired) {
                                float acc = std::max(0.1f, state.entities.get(*state.player_vid)->stats.accuracy / 100.0f);
                                float base_dev = (gd ? gd->deviation : 0.0f) / acc;
                                float move_spread = state.entities.get(*state.player_vid)->move_spread_deg / acc;
                                float recoil_spread = gim->spread_recoil_deg;
                                float theta_deg = std::clamp(base_dev + move_spread + recoil_spread, MIN_SPREAD_DEG, MAX_SPREAD_DEG);
                                // uniform random in [-theta, +theta]
                                static thread_local std::mt19937 rng_theta{std::random_device{}()};
                                std::uniform_real_distribution<float> Uphi(-theta_deg, theta_deg);
                                float phi = Uphi(rng_theta) * 3.14159265358979323846f / 180.0f;
                                float cs = std::cos(phi), sn = std::sin(phi);
                                glm::vec2 rdir{aim.x * cs - aim.y * sn, aim.x * sn + aim.y * cs};
                                dir = glm::normalize(rdir);
                            }
                            // jam chance
                            if (fired) {
                                static thread_local std::mt19937 rng{std::random_device{}()};
                                std::uniform_real_distribution<float> U(0.0f, 1.0f);
                                float jc = state.base_jam_chance + (gd ? gd->jam_chance : 0.0f);
                                jc = std::clamp(jc, 0.0f, 1.0f);
                                if (U(rng) < jc) {
                                    gim->jammed = true;
                                    gim->unjam_progress = 0.0f;
                                    fired = false;
                            if (g_lua_mgr)
                                g_lua_mgr->call_gun_on_jam(gim->def_type, state, *plm);
                            sounds.play(gd->sound_jam.empty() ? "base:ui_cant"
                                                                      : gd->sound_jam);
                            state.alerts.push_back(
                                {"Gun jammed! Mash SPACE", 0.0f, 2.0f, false});
                            // Metrics
                            if (state.player_vid) {
                                if (auto* pm = state.metrics_for(*state.player_vid))
                                    pm->jams += 1;
                            }
                                }
                            }
                            // Metrics: shots fired (only if we attempted and had ammo / not blocked)
                            if (fired && state.player_vid) {
                                if (auto* pm = state.metrics_for(*state.player_vid))
                                    pm->shots_fired += 1;
                            }
                            // Accumulate recoil spread on shot
                            if (fired && gd && gim) {
                                gim->spread_recoil_deg = std::min(gim->spread_recoil_deg + gd->recoil,
                                                                 gd->max_recoil_spread_deg);
                            }
                        }
                    }
                }
                if (fired) {
                    // spawn pellets at gun muzzle, tag owner
                    int pellets = 1;
                    if (state.player_vid) {
                        auto* plm = state.entities.get_mut(*state.player_vid);
                        if (plm && plm->equipped_gun_vid.has_value()) {
                            if (const GunInstance* gi = state.guns.get(*plm->equipped_gun_vid)) {
                                const GunDef* gd = nullptr;
                                if (g_lua_mgr) {
                                    for (auto const& g : g_lua_mgr->guns())
                                        if (g.type == gi->def_type) { gd = &g; break; }
                                }
                                if (gd && gd->pellets_per_shot > 1)
                                    pellets = gd->pellets_per_shot;
                            }
                        }
                    }
                    // Compute spread angle once per shot
                    float theta_deg_for_shot = 0.0f;
                    if (state.player_vid) {
                        auto* plm = state.entities.get_mut(*state.player_vid);
                        if (plm && plm->equipped_gun_vid.has_value()) {
                            if (const GunInstance* gi = state.guns.get(*plm->equipped_gun_vid)) {
                                const GunDef* gd = nullptr;
                                if (g_lua_mgr) {
                                    for (auto const& g : g_lua_mgr->guns())
                                        if (g.type == gi->def_type) { gd = &g; break; }
                                }
                                if (gd) {
                                    float acc = std::max(0.1f, plm->stats.accuracy / 100.0f);
                                    float base_dev = gd->deviation / acc;
                                    float move_spread = plm->move_spread_deg / acc;
                                    float recoil_spread = const_cast<GunInstance*>(gi)->spread_recoil_deg;
                                    theta_deg_for_shot = std::clamp(base_dev + move_spread + recoil_spread,
                                                                    MIN_SPREAD_DEG, MAX_SPREAD_DEG);
                                }
                            }
                        }
                    }
                    static thread_local std::mt19937 rng_theta2{std::random_device{}()};
                    std::uniform_real_distribution<float> Uphi2(-theta_deg_for_shot, theta_deg_for_shot);
                    for (int i = 0; i < pellets; ++i) {
                        float phi = Uphi2(rng_theta2) * 3.14159265358979323846f / 180.0f;
                        float cs = std::cos(phi), sn = std::sin(phi);
                        glm::vec2 pdir{aim.x * cs - aim.y * sn, aim.x * sn + aim.y * cs};
                        pdir = glm::normalize(pdir);
                        glm::vec2 sp = p + pdir * GUN_MUZZLE_OFFSET_UNITS;
                        auto* pr = projectiles.spawn(sp, pdir * proj_speed, proj_size, proj_steps, proj_type);
                        if (pr && state.player_vid)
                            pr->owner = state.player_vid;
                        if (pr) {
                            pr->sprite_id = proj_sprite_id;
                            pr->ammo_type = ammo_type;
                            // Base damage from gun, scaled by ammo (if any)
                            float base_dmg = 1.0f;
                            if (g_lua_mgr && state.player_vid) {
                                // resolve gun def again for damage
                                if (auto* plmm = state.entities.get_mut(*state.player_vid)) {
                                    if (plmm->equipped_gun_vid.has_value()) {
                                        if (const GunInstance* gi2 = state.guns.get(*plmm->equipped_gun_vid)) {
                                            const GunDef* gd2 = nullptr;
                                            for (auto const& g : g_lua_mgr->guns()) if (g.type == gi2->def_type) { gd2 = &g; break; }
                                            if (gd2) base_dmg = gd2->damage;
                                        }
                                    }
                                }
                            }
                            float dmg_mult = 1.0f, armor_pen = 0.0f, shield_mult = 1.0f, range_units = 0.0f;
                            if (g_lua_mgr && ammo_type != 0) {
                                if (auto const* ad = g_lua_mgr->find_ammo(ammo_type)) {
                                    dmg_mult = ad->damage_mult;
                                    armor_pen = ad->armor_pen;
                                    shield_mult = ad->shield_mult;
                                    range_units = ad->range_units;
                                    if (pr)
                                        pr->pierce_remaining = std::max(0, ad->pierce_count);
                                }
                            }
                            pr->base_damage = base_dmg * dmg_mult;
                            pr->armor_pen = armor_pen;
                            pr->shield_mult = shield_mult;
                            pr->max_range_units = range_units;
                        }
                        (void)pr;
                    }
                    // play fire sound (gun-specific or fallback)
                    if (state.player_vid) {
                        auto* plm = state.entities.get_mut(*state.player_vid);
                        if (plm && plm->equipped_gun_vid.has_value()) {
                            const GunInstance* gi = state.guns.get(*plm->equipped_gun_vid);
                            const GunDef* gd = nullptr;
                            if (g_lua_mgr && gi) {
                                for (auto const& g : g_lua_mgr->guns())
                                    if (g.type == gi->def_type) {
                                        gd = &g;
                                        break;
                                    }
                            }
                            sounds.play((gd && !gd->sound_fire.empty()) ? gd->sound_fire
                                                                        : "base:small_shoot");
                        } else {
                            sounds.play("base:small_shoot");
                        }
                    } else {
                        sounds.play("base:small_shoot");
                    }
                    // on_shoot triggers for items in inventory
                    if (g_lua_mgr && state.player_vid) {
                        auto* plm = state.entities.get_mut(*state.player_vid);
                        if (plm) {
                            for (const auto& entry : state.inventory.entries) {
                                if (entry.kind == INV_ITEM) {
                                    if (const ItemInstance* inst = state.items.get(entry.vid)) {
                                        g_lua_mgr->call_item_on_shoot(inst->def_type, state, *plm);
                                    }
                                }
                            }
                        }
                    }
                    // set cooldown; for burst, use burst cadence
                    if (state.player_vid && fire_mode == "burst" && burst_step &&
                        burst_rpm > 0.0f) {
                        state.gun_cooldown = std::max(0.01f, 60.0f / burst_rpm);
                        // decrement burst
                        auto* plm2 = state.entities.get_mut(*state.player_vid);
                        if (plm2 && plm2->equipped_gun_vid.has_value()) {
                            if (auto* gim2 = state.guns.get(*plm2->equipped_gun_vid)) {
                                gim2->burst_remaining = std::max(0, gim2->burst_remaining - 1);
                                gim2->burst_timer = state.gun_cooldown;
                            }
                        }
                    } else {
                        state.gun_cooldown = std::max(0.05f, 60.0f / rpm);
                        // end burst window if we just shot last burst or not in burst
                        if (state.player_vid && fire_mode == "burst") {
                            auto* plm2 = state.entities.get_mut(*state.player_vid);
                            if (plm2 && plm2->equipped_gun_vid.has_value()) {
                                auto* gim2 = state.guns.get(*plm2->equipped_gun_vid);
                                if (gim2 && gim2->burst_remaining == 0)
                                    gim2->burst_timer = 0.0f;
                            }
                        }
                    }
                }
            }

            // Unjam handling: mash SPACE to clear jam; reload on success if ammo present
            if (state.mode == ids::MODE_PLAYING && state.player_vid) {
                auto* plm = state.entities.get_mut(*state.player_vid);
                if (plm && plm->equipped_gun_vid.has_value()) {
                    GunInstance* gim = state.guns.get(*plm->equipped_gun_vid);
                    if (gim && gim->jammed) {
                        static bool prev_space = false;
                        bool now_space = state.playing_inputs.use_center;
                        if (now_space && !prev_space) {
                            state.reticle_shake = std::max(state.reticle_shake, 20.0f);
                            gim->unjam_progress = std::min(1.0f, gim->unjam_progress + 0.2f);
                            // Metrics: count unjam mashes
                            if (state.player_vid) {
                                if (auto* pm = state.metrics_for(*state.player_vid))
                                    pm->unjam_mashes += 1;
                            }
                        }
                        prev_space = now_space;
                        if (gim->unjam_progress >= 1.0f) {
                            gim->jammed = false;
                            gim->unjam_progress = 0.0f;
                            state.reticle_shake = std::max(state.reticle_shake, 10.0f);
                            // attempt reload on unjam
                            const GunDef* gd = nullptr;
                            if (g_lua_mgr) {
                                for (auto const& g : g_lua_mgr->guns())
                                    if (g.type == gim->def_type) {
                                        gd = &g;
                                        break;
                                    }
                            }
                            if (gd) {
                                if (gim->ammo_reserve > 0) {
                                    int dropped = gim->current_mag;
                                    if (dropped > 0)
                                        state.alerts.push_back({std::string("Dropped ") +
                                                                    std::to_string(dropped) +
                                                                    " bullets",
                                                                0.0f, 1.5f, false});
                                    // start active reload
                                    gim->current_mag = 0;
                                    gim->reloading = true;
                                    gim->reload_progress = 0.0f;
                                    gim->reload_eject_remaining = std::max(0.0f, gd->eject_time);
                                    gim->reload_total_time = std::max(0.1f, gd->reload_time);
                                    static thread_local std::mt19937 rng2{std::random_device{}()};
                                    auto clamp01b = [](float v) {
                                        return std::max(0.0f, std::min(1.0f, v));
                                    };
                                    std::uniform_real_distribution<float> Upos2(
                                        -gd->ar_pos_variance, gd->ar_pos_variance);
                                    std::uniform_real_distribution<float> Usize2(
                                        -gd->ar_size_variance, gd->ar_size_variance);
                                    float size2 =
                                        std::clamp(gd->ar_size + Usize2(rng2), 0.02f, 0.9f);
                                    float center2 = clamp01b(gd->ar_pos + Upos2(rng2));
                                    float start2 = clamp01b(center2 - size2 * 0.5f);
                                    if (start2 + size2 > 1.0f)
                                        start2 = 1.0f - size2;
                                    gim->ar_window_start = start2;
                                    gim->ar_window_end = start2 + size2;
                                    gim->ar_consumed = false;
                                    state.alerts.push_back(
                                        {"Unjammed: Reloading...", 0.0f, 1.0f, false});
                                    sounds.play("base:unjam");
                                } else {
                                    state.alerts.push_back(
                                        {"Unjammed: NO AMMO", 0.0f, 1.5f, false});
                                }
                            }
                        }
                    }
                }
            }

            // step projectiles with on-hit applying damage and drops
            sim_step_projectiles(state, projectiles);
            // After projectile resolution
            /* for (auto h : hits) {
                auto id = h.eid;
                if (id >= state.entities.data().size())
                    continue;
                auto& e = state.entities.data()[id];
                if (!e.active)
                    continue;
                if (e.type_ == ids::ET_NPC || e.type_ == ids::ET_PLAYER) {
                    if (e.health == 0)
                        e.health = 3; // initialize default
                    if (e.max_hp == 0)
                        e.max_hp = 3;
                    // Apply damage with shields, plates, armor using projectile/ammo parameters
                    float dmg = h.base_damage;
                    float ap = std::clamp(h.armor_pen * 100.0f, 0.0f, 100.0f);
                    float shield_mult = h.shield_mult;
                    // Apply distance falloff from ammo if defined
                    if (g_lua_mgr && h.ammo_type != 0) {
                        if (auto const* ad = g_lua_mgr->find_ammo(h.ammo_type)) {
                            if (ad->falloff_end > ad->falloff_start && ad->falloff_end > 0.0f) {
                                float m = 1.0f;
                                if (h.travel_dist <= ad->falloff_start) m = 1.0f;
                                else if (h.travel_dist >= ad->falloff_end) m = ad->falloff_min_mult;
                                else {
                                    float t = (h.travel_dist - ad->falloff_start) / (ad->falloff_end - ad->falloff_start);
                                    m = 1.0f + t * (ad->falloff_min_mult - 1.0f);
                                }
                                if (m < 0.0f) m = 0.0f;
                                dmg *= m;
                            }
                        }
                    }
                    // Fallback damage if none resolved
                    if (dmg <= 0.0f)
                        dmg = 1.0f;
                    if (e.type_ == ids::ET_PLAYER) {
                        // For players: shields first, then plates, then HP with armor
                        if (e.stats.shield_max > 0.0f && e.shield > 0.0f) {
                            float took = std::min(e.shield, (float)(dmg * shield_mult));
                            e.shield -= took;
                            // metrics: damage to shield
                            if (auto* pm = state.metrics_for(e.vid))
                                pm->damage_taken_shield += (std::uint64_t)std::lround(took);
                            // still counts as dealt damage for owner
                            if (h.owner) {
                                if (auto* om = state.metrics_for(*h.owner))
                                    om->damage_dealt += (std::uint64_t)std::lround(took);
                            }
                            dmg -= took; // remaining damage continues to plates/HP
                            if (dmg < 0.0f)
                                dmg = 0.0f;
                        }
                        if (dmg > 0.0f && e.stats.plates > 0) {
                            e.stats.plates -= 1;
                            // metrics: plate consumed
                            if (auto* pm = state.metrics_for(e.vid))
                                pm->plates_consumed += 1;
                            dmg = 0.0f;
                        }
                        if (dmg > 0.0f) {
                            float reduction = std::max(0.0f, e.stats.armor - (float)ap);
                            reduction = std::min(75.0f, reduction);
                            float scale = 1.0f - reduction * 0.01f;
                            int delt = (int)std::ceil((double)dmg * (double)scale);
                            std::uint32_t before = e.health;
                            e.health = (e.health > (uint32_t)delt) ? (e.health - (uint32_t)delt) : 0u;
                            // metrics: damage to HP
                            if (auto* pm = state.metrics_for(e.vid))
                                pm->damage_taken_hp += (std::uint64_t)(before - e.health);
                            // attribute as dealt
                            if (h.owner) {
                                if (auto* om = state.metrics_for(*h.owner))
                                    om->damage_dealt += (std::uint64_t)delt;
                            }
                        }
                    } else {
                        // NPC path (existing logic)
                        if (e.stats.plates > 0) {
                            e.stats.plates -= 1;
                            dmg = 0.0f;
                        }
                        if (dmg > 0.0f) {
                            float reduction = std::max(0.0f, e.stats.armor - (float)ap);
                            reduction = std::min(75.0f, reduction); // cap benefit
                            float scale = 1.0f - reduction * 0.01f;
                            int delt = (int)std::ceil((double)dmg * (double)scale);
                            e.health = (e.health > (uint32_t)delt) ? (e.health - (uint32_t)delt) : 0u;
                            // Attribute damage dealt to owner if any
                            if (h.owner) {
                                if (auto* pm = state.metrics_for(*h.owner))
                                    pm->damage_dealt += (std::uint64_t)delt;
                            }
                        }
                    }
                    // mark damage time (for shield regen delay if applicable)
                    e.time_since_damage = 0.0f;
                    if (e.type_ == ids::ET_NPC && e.health == 0) {
                        // drop chance
                        glm::vec2 pos = e.pos;
                        e.active = false;
                        // Metrics: kills
                        state.metrics.enemies_slain += 1;
                        state.metrics.enemies_slain_by_type[(int)e.type_] += 1;
                        if (h.owner) {
                            if (auto* pm = state.metrics_for(*h.owner))
                                pm->enemies_slain += 1;
                        }
                        // basic drop: 50% chance to drop something
                        static thread_local std::mt19937 rng{std::random_device{}()};
                        std::uniform_real_distribution<float> U(0.0f, 1.0f);
                        if (U(rng) < 0.5f && g_lua_mgr) {
                            glm::vec2 place_pos = ensure_not_in_block(state, pos);
                            const auto& dt = g_lua_mgr->drops();
                            auto pick_weighted = [&](const std::vector<DropEntry>& v) -> int {
                                if (v.empty())
                                    return -1;
                                float sum = 0.0f;
                                for (auto const& de : v)
                                    sum += de.weight;
                                if (sum <= 0.0f)
                                    return -1;
                                std::uniform_real_distribution<float> du(0.0f, sum);
                                float r = du(rng);
                                float acc = 0.0f;
                                for (auto const& de : v) {
                                    acc += de.weight;
                                    if (r <= acc)
                                        return de.type;
                                }
                                return v.back().type;
                            };
                            if (!dt.powerups.empty() || !dt.items.empty() || !dt.guns.empty()) {
                                float c = U(rng);
                                if (c < 0.5f && !dt.powerups.empty()) {
                                    int t = pick_weighted(dt.powerups);
                                    if (t >= 0) {
                                        auto it = std::find_if(
                                            g_lua_mgr->powerups().begin(),
                                            g_lua_mgr->powerups().end(),
                                            [&](const PowerupDef& p) { return p.type == t; });
                                        if (it != g_lua_mgr->powerups().end()) {
                                            auto* p = state.pickups.spawn((std::uint32_t)it->type,
                                                                          it->name, place_pos);
                                            if (p) {
                                                state.metrics.powerups_spawned += 1;
                                                if (g_sprite_ids) {
                                                    if (!it->sprite.empty() &&
                                                        it->sprite.find(':') != std::string::npos)
                                                        p->sprite_id =
                                                            g_sprite_ids->try_get(it->sprite);
                                                    else
                                                        p->sprite_id = -1;
                                                }
                                            }
                                        }
                                    }
                                } else if (c < 0.85f && !dt.items.empty()) {
                                    int t = pick_weighted(dt.items);
                                    if (t >= 0) {
                                        auto it = std::find_if(
                                            g_lua_mgr->items().begin(), g_lua_mgr->items().end(),
                                            [&](const ItemDef& d) { return d.type == t; });
                                        if (it != g_lua_mgr->items().end()) {
                                            auto iv = state.items.spawn_from_def(*it, 1);
                                            if (iv) {
                                                state.ground_items.spawn(*iv, place_pos);
                                                state.metrics.items_spawned += 1;
                                            }
                                        }
                                    }
                                } else if (!dt.guns.empty()) {
                                    int t = pick_weighted(dt.guns);
                                    if (t >= 0) {
                                        auto itg = std::find_if(
                                            g_lua_mgr->guns().begin(), g_lua_mgr->guns().end(),
                                            [&](const GunDef& g) { return g.type == t; });
                                        if (itg != g_lua_mgr->guns().end()) {
                                            auto inst = state.guns.spawn_from_def(*itg);
                                            int gspr = -1;
                                            if (g_sprite_ids) {
                                                if (!itg->sprite.empty() &&
                                                    itg->sprite.find(':') != std::string::npos)
                                                    gspr = g_sprite_ids->try_get(itg->sprite);
                                                else
                                                    gspr = -1;
                                            }
                                            if (inst) {
                                                state.ground_guns.spawn(*inst, place_pos, gspr);
                                                state.metrics.guns_spawned += 1;
                                            }
                                        }
                                    }
                                }
                            } else if (U(rng) < 0.5f && !g_lua_mgr->powerups().empty()) {
                                std::uniform_int_distribution<int> di(
                                    0, (int)g_lua_mgr->powerups().size() - 1);
                                auto& pu = g_lua_mgr->powerups()[(size_t)di(rng)];
                                auto* p = state.pickups.spawn((std::uint32_t)pu.type, pu.name, place_pos);
                                if (p) {
                                    state.metrics.powerups_spawned += 1;
                                    if (!pu.sprite.empty() &&
                                        pu.sprite.find(':') != std::string::npos)
                                        p->sprite_id = sprites.try_get(pu.sprite);
                                    else
                                        p->sprite_id = -1;
                                }
                            } else if (U(rng) < 0.8f && !g_lua_mgr->items().empty()) {
                                std::uniform_int_distribution<int> di2(
                                    0, (int)g_lua_mgr->items().size() - 1);
                                auto idf = g_lua_mgr->items()[(size_t)di2(rng)];
                                auto iv = state.items.spawn_from_def(idf, 1);
                                if (iv)
                                    if (state.ground_items.spawn(*iv, place_pos))
                                        state.metrics.items_spawned += 1;
                            } else if (!g_lua_mgr->guns().empty()) {
                                std::uniform_int_distribution<int> di3(
                                    0, (int)g_lua_mgr->guns().size() - 1);
                                auto gd = g_lua_mgr->guns()[(size_t)di3(rng)];
                                auto inst = state.guns.spawn_from_def(gd);
                                int gspr = -1;
                                if (g_sprite_ids) {
                                    if (!gd.sprite.empty() &&
                                        gd.sprite.find(':') != std::string::npos)
                                        gspr = g_sprite_ids->try_get(gd.sprite);
                                    else
                                        gspr = -1;
                                }
                                if (inst)
                                    if (state.ground_guns.spawn(*inst, place_pos, gspr))
                                        state.metrics.guns_spawned += 1;
                            }
                        }
                    }
                }
            } */

            // After-physics ticking (opt-in)
            if (state.player_vid && g_lua_mgr) {
                Entity* plat = state.entities.get_mut(*state.player_vid);
                if (plat) {
                    const float dt = TIMESTEP;
                    const int MAX_TICKS = 4000;
                    int tick_calls = 0;
                    // Guns with on_step
                    for (const auto& entry : state.inventory.entries) {
                        if (entry.kind != INV_GUN)
                            continue;
                        GunInstance* gi = state.guns.get(entry.vid);
                        if (!gi)
                            continue;
                        const GunDef* gd = nullptr;
                        for (auto const& g : g_lua_mgr->guns())
                            if (g.type == gi->def_type) {
                                gd = &g;
                                break;
                            }
                        if (!gd || gd->on_step_ref < 0)
                            continue;
                        if (gd->tick_rate_hz <= 0.0f || gd->tick_phase == "before")
                            continue;
                        gi->tick_acc += dt;
                        float period = 1.0f / std::max(1.0f, gd->tick_rate_hz);
                        while (gi->tick_acc >= period && tick_calls < MAX_TICKS) {
                            g_lua_mgr->call_gun_on_step(gi->def_type, state, *plat);
                            gi->tick_acc -= period;
                            ++tick_calls;
                        }
                    }
                    // Items with on_tick
                    for (const auto& entry : state.inventory.entries) {
                        if (entry.kind != INV_ITEM)
                            continue;
                        ItemInstance* inst = state.items.get(entry.vid);
                        if (!inst)
                            continue;
                        const ItemDef* idf = nullptr;
                        for (auto const& d : g_lua_mgr->items())
                            if (d.type == inst->def_type) {
                                idf = &d;
                                break;
                            }
                        if (!idf || idf->on_tick_ref < 0)
                            continue;
                        if (idf->tick_rate_hz <= 0.0f || idf->tick_phase == "before")
                            continue;
                        inst->tick_acc += dt;
                        float period = 1.0f / std::max(1.0f, idf->tick_rate_hz);
                        while (inst->tick_acc >= period && tick_calls < MAX_TICKS) {
                            g_lua_mgr->call_item_on_tick(inst->def_type, state, *plat, period);
                            inst->tick_acc -= period;
                            ++tick_calls;
                        }
                    }
                }
            }
        }

        // Allow proceeding from score review after delay
        if (state.mode == ids::MODE_SCORE_REVIEW && state.score_ready_timer <= 0.0f) {
            if (state.menu_inputs.confirm || state.playing_inputs.use_center || state.mouse_inputs.left) {
                // Cleanup ground instances (free orphans)
                for (auto& gi : state.ground_items.data())
                    if (gi.active) {
                        state.items.free(gi.item_vid);
                        gi.active = false;
                    }
                for (auto& gg : state.ground_guns.data())
                    if (gg.active) {
                        state.guns.free(gg.gun_vid);
                        gg.active = false;
                    }
                std::printf("[room] Proceeding to next area info screen.\n");
                state.mode = ids::MODE_NEXT_STAGE;
                state.score_ready_timer = 0.5f; // brief delay before allowing confirm
                state.input_lockout_timer = 0.2f; // suppress click-through actions
            }
        }
        // Proceed from next-stage info to actual next area
        if (state.mode == ids::MODE_NEXT_STAGE && state.score_ready_timer <= 0.0f) {
            if (state.menu_inputs.confirm || state.playing_inputs.use_center || state.mouse_inputs.left) {
                std::printf("[room] Entering next area.\n");
                state.alerts.push_back({"Entering next area", 0.0f, 2.0f, false});
                state.mode = ids::MODE_PLAYING;
                generate_room(state, projectiles, renderer, gfx);
                state.input_lockout_timer = 0.25f; // avoid firing immediately after click
            }
        }
        
        // Simulation-side crate progression
        sim_update_crates_open(state);
        // Render a full frame via renderer module
        render_frame(window, renderer, textures, ui_font, state, gfx, dt_sec, binds, projectiles, sounds);
        // Legacy inlined rendering kept below temporarily (disabled)
        #if 0
        
        // Paint a background every frame
        if (renderer) {
            SDL_SetRenderDrawColor(renderer, 18, 18, 20, 255); // dark gray
            SDL_RenderClear(renderer);
            // One-frame warnings (e.g., missing sprites), rendered in red.
            std::vector<std::string> frame_warnings;
            auto add_warning = [&](const std::string& s) {
                if (std::find(frame_warnings.begin(), frame_warnings.end(), s) ==
                    frame_warnings.end())
                    frame_warnings.push_back(s);
            };
            // Fetch output size each frame
            SDL_GetRendererOutputSize(renderer, &width, &height);
            auto world_to_screen = [&](float wx, float wy) -> SDL_FPoint {
                float scale = TILE_SIZE * gfx.play_cam.zoom;
                float sx = (wx - gfx.play_cam.pos.x) * scale + static_cast<float>(width) * 0.5f;
                float sy = (wy - gfx.play_cam.pos.y) * scale + static_cast<float>(height) * 0.5f;
                return SDL_FPoint{sx, sy};
            };

            if (state.mode == ids::MODE_PLAYING) {
                // draw tiles
                for (int y = 0; y < (int)state.stage.get_height(); ++y) {
                for (int x = 0; x < (int)state.stage.get_width(); ++x) {
                    const auto& t = state.stage.at(x, y);
                    bool is_start = (x == state.start_tile.x && y == state.start_tile.y);
                    bool is_exit = (x == state.exit_tile.x && y == state.exit_tile.y);
                    if (t.blocks_entities() || t.blocks_projectiles() || is_start || is_exit) {
                        if (is_start)
                            SDL_SetRenderDrawColor(renderer, 80, 220, 90, 255);
                        else if (is_exit)
                            SDL_SetRenderDrawColor(renderer, 240, 220, 80, 255);
                        else if (t.blocks_entities() && t.blocks_projectiles())
                            SDL_SetRenderDrawColor(renderer, 90, 90, 90, 255); // wall
                        else if (t.blocks_entities() && !t.blocks_projectiles())
                            SDL_SetRenderDrawColor(renderer, 70, 90, 160, 255); // void/water
                        else
                            SDL_SetRenderDrawColor(renderer, 90, 90, 90, 255);
                        SDL_FPoint p0 =
                            world_to_screen(static_cast<float>(x), static_cast<float>(y));
                        float scale = TILE_SIZE * gfx.play_cam.zoom;
                        SDL_Rect tr{(int)std::floor(p0.x), (int)std::floor(p0.y),
                                    (int)std::ceil(scale), (int)std::ceil(scale)};
                        SDL_RenderFillRect(renderer, &tr);
                    }
                }
                }
            }
            // Crate open/loot progression handled in sim now
            sim_update_crates_open(state);
            // Draw crates (visuals only)
            if (state.mode == ids::MODE_PLAYING) {
                for (auto& c : state.crates.data())
                    if (c.active && !c.opened) {
                        glm::vec2 ch = c.size * 0.5f;
                        // world → screen rect
                        SDL_FPoint c0 = world_to_screen(c.pos.x - ch.x, c.pos.y - ch.y);
                        float scale = TILE_SIZE * gfx.play_cam.zoom;
                        SDL_Rect rc{(int)std::floor(c0.x), (int)std::floor(c0.y),
                                    (int)std::ceil(c.size.x * scale),
                                    (int)std::ceil(c.size.y * scale)};
                        SDL_SetRenderDrawColor(renderer, 120, 80, 40, 255);
                        SDL_RenderFillRect(renderer, &rc);
                        SDL_SetRenderDrawColor(renderer, 200, 160, 100, 255);
                        SDL_RenderDrawRect(renderer, &rc);
                        // label above
                        if (ui_font && g_lua_mgr) {
                            std::string label = "Crate";
                            if (auto const* cd = g_lua_mgr->find_crate(c.def_type))
                                label = cd->label.empty() ? cd->name : cd->label;
                            SDL_Color lc{240, 220, 80, 255};
                            SDL_Surface* lsrf = TTF_RenderUTF8_Blended(ui_font, label.c_str(), lc);
                            if (lsrf) {
                                SDL_Texture* lt = SDL_CreateTextureFromSurface(renderer, lsrf);
                                int tw = 0, th = 0;
                                SDL_QueryTexture(lt, nullptr, nullptr, &tw, &th);
                                SDL_Rect ld{rc.x + (rc.w - tw) / 2, rc.y - th - 18, tw, th};
                                SDL_RenderCopy(renderer, lt, nullptr, &ld);
                                SDL_DestroyTexture(lt);
                                SDL_FreeSurface(lsrf);
                            }
                        }
                        // progress bar above (visual ratio from open_progress/open_time)
                        float open_time = 5.0f;
                        if (g_lua_mgr) {
                            if (auto const* cd = g_lua_mgr->find_crate(c.def_type))
                                open_time = cd->open_time;
                        }
                        int bw = rc.w;
                        int bh = 8;
                        int bx = rc.x;
                        int by = rc.y - 14;
                        SDL_Rect pbg{bx, by, bw, bh};
                        SDL_SetRenderDrawColor(renderer, 30, 30, 30, 200);
                        SDL_RenderFillRect(renderer, &pbg);
                        int fw = (int)std::lround((double)bw *
                                                  (double)(c.open_progress / std::max(0.0001f, open_time)));
                        fw = std::clamp(fw, 0, bw);
                        SDL_Rect pfg{bx, by, fw, bh};
                        SDL_SetRenderDrawColor(renderer, 240, 220, 80, 230);
                        SDL_RenderFillRect(renderer, &pfg);
                    }
            }
            // draw entities (only during gameplay)
            if (state.mode == ids::MODE_PLAYING)
            for (auto const& e : state.entities.data()) {
                if (!e.active)
                    continue;
                // sprite if available
                bool drew_sprite = false;
                if (e.sprite_id >= 0) {
                    SDL_Texture* tex = textures.get(e.sprite_id);
                    if (tex) {
                        int tw = 0, th = 0;
                        SDL_QueryTexture(tex, nullptr, nullptr, &tw, &th);
                        SDL_FPoint c =
                            world_to_screen(e.pos.x - e.size.x * 0.5f, e.pos.y - e.size.y * 0.5f);
                        float scale = TILE_SIZE * gfx.play_cam.zoom;
                        SDL_Rect dst{(int)std::floor(c.x), (int)std::floor(c.y),
                                     (int)std::ceil(e.size.x * scale),
                                     (int)std::ceil(e.size.y * scale)};
                        SDL_RenderCopy(renderer, tex, nullptr, &dst);
                        drew_sprite = true;
                    } else {
                        add_warning("Missing texture for entity sprite");
                    }
                }
                // debug AABB
                if (!drew_sprite) {
                    add_warning("Missing sprite for entity");
                    if (e.type_ == ids::ET_PLAYER)
                        SDL_SetRenderDrawColor(renderer, 60, 140, 240, 255);
                    else if (e.type_ == ids::ET_NPC)
                        SDL_SetRenderDrawColor(renderer, 220, 60, 60, 255);
                    else
                        SDL_SetRenderDrawColor(renderer, 180, 180, 200, 255);
                    SDL_FPoint c =
                        world_to_screen(e.pos.x - e.size.x * 0.5f, e.pos.y - e.size.y * 0.5f);
                    float scale = TILE_SIZE * gfx.play_cam.zoom;
                    SDL_Rect r{(int)std::floor(c.x), (int)std::floor(c.y),
                               (int)std::ceil(e.size.x * scale), (int)std::ceil(e.size.y * scale)};
                    SDL_RenderFillRect(renderer, &r);
                } else {
                    // draw outline AABB for debug
                    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 60);
                    SDL_FPoint c =
                        world_to_screen(e.pos.x - e.size.x * 0.5f, e.pos.y - e.size.y * 0.5f);
                    float scale = TILE_SIZE * gfx.play_cam.zoom;
                    SDL_Rect r{(int)std::floor(c.x), (int)std::floor(c.y),
                               (int)std::ceil(e.size.x * scale), (int)std::ceil(e.size.y * scale)};
                    SDL_RenderDrawRect(renderer, &r);
                }
                // If player and has equipped gun, render gun rotated around player
                if (e.type_ == ids::ET_PLAYER) {
                    const Entity* pl = &e;
                    if (pl->equipped_gun_vid.has_value() && g_lua_mgr) {
                        const GunInstance* gi = state.guns.get(*pl->equipped_gun_vid);
                        const GunDef* gd = nullptr;
                        if (gi) {
                            for (auto const& g : g_lua_mgr->guns())
                                if (g.type == gi->def_type) {
                                    gd = &g;
                                    break;
                                }
                        }
                        int gspr = -1;
                        if (gd && g_sprite_ids) {
                            if (!gd->sprite.empty() && gd->sprite.find(':') != std::string::npos)
                                gspr = g_sprite_ids->try_get(gd->sprite);
                        }
                        // Compute aim angle and position
                        int ww = width, wh = height;
                        SDL_GetRendererOutputSize(renderer, &ww, &wh);
                        float inv_scale = 1.0f / (TILE_SIZE * gfx.play_cam.zoom);
                        glm::vec2 m = {
                            gfx.play_cam.pos.x + (static_cast<float>(state.mouse_inputs.pos.x) -
                                                  static_cast<float>(ww) * 0.5f) *
                                                     inv_scale,
                            gfx.play_cam.pos.y + (static_cast<float>(state.mouse_inputs.pos.y) -
                                                  static_cast<float>(wh) * 0.5f) *
                                                     inv_scale};
                        glm::vec2 dir = glm::normalize(m - pl->pos);
                        if (glm::any(glm::isnan(dir)))
                            dir = {1.0f, 0.0f};
                        float angle_deg = std::atan2(dir.y, dir.x) * 180.0f / 3.14159265f;
                        glm::vec2 gun_pos = pl->pos + dir * GUN_HOLD_OFFSET_UNITS;
                        SDL_FPoint c0 = world_to_screen(gun_pos.x - 0.15f, gun_pos.y - 0.10f);
                        float scale = TILE_SIZE * gfx.play_cam.zoom;
                        SDL_Rect r{(int)std::floor(c0.x), (int)std::floor(c0.y),
                                   (int)std::ceil(0.30f * scale), (int)std::ceil(0.20f * scale)};
                        if (gspr >= 0) {
                            if (SDL_Texture* tex = textures.get(gspr))
                                SDL_RenderCopyEx(renderer, tex, nullptr, &r, angle_deg, nullptr,
                                                 SDL_FLIP_NONE);
                            else
                                add_warning("Missing texture for held gun sprite");
                        } else {
                            add_warning("Missing sprite for held gun");
                            SDL_SetRenderDrawColor(renderer, 180, 180, 200, 255);
                            SDL_RenderFillRect(renderer, &r);
                        }
                    }
                }
            }

            // Enemy health bars above heads (for damaged NPCs)
            if (state.mode == ids::MODE_PLAYING)
            for (auto const& e : state.entities.data()) {
                if (!e.active || e.type_ != ids::ET_NPC)
                    continue;
                if (e.max_hp == 0 || e.health >= e.max_hp)
                    continue;
                SDL_FPoint c =
                    world_to_screen(e.pos.x - e.size.x * 0.5f, e.pos.y - e.size.y * 0.5f);
                float scale = TILE_SIZE * gfx.play_cam.zoom;
                int w = (int)std::ceil(e.size.x * scale);
                int h = 6;
                SDL_Rect bg{(int)std::floor(c.x), (int)std::floor(c.y) - (h + 4), w, h};
                SDL_SetRenderDrawColor(renderer, 30, 30, 34, 220);
                SDL_RenderFillRect(renderer, &bg);
                SDL_SetRenderDrawColor(renderer, 80, 80, 90, 255);
                SDL_RenderDrawRect(renderer, &bg);
                float ratio = (float)e.health / (float)e.max_hp;
                ratio = std::clamp(ratio, 0.0f, 1.0f);
                int hw = (int)std::lround((double)w * (double)ratio);
                SDL_Rect hr{bg.x, bg.y, hw, bg.h};
                // Optional shield indicator as thin cyan band
                if (e.stats.shield_max > 0.0f && e.shield > 0.0f) {
                    float sratio = std::clamp(e.shield / e.stats.shield_max, 0.0f, 1.0f);
                    int sw = (int)std::lround((double)w * (double)sratio);
                    int sh = std::max(2, h / 3);
                    SDL_Rect sb{bg.x, bg.y + 1, sw, sh};
                    SDL_SetRenderDrawColor(renderer, 120, 200, 240, 160);
                    SDL_RenderFillRect(renderer, &sb);
                }
                SDL_SetRenderDrawColor(renderer, 220, 60, 60, 230);
                SDL_RenderFillRect(renderer, &hr);
                // plates as thin slivers aligned from right edge
                int slw = 2;
                int gap = 1;
                for (int i = 0; i < e.stats.plates; ++i) {
                    int rx = bg.x + w - (i + 1) * (slw + gap);
                    SDL_Rect pr{rx, bg.y, slw, bg.h};
                    SDL_SetRenderDrawColor(renderer, 50, 50, 50, 255);
                    SDL_RenderFillRect(renderer, &pr);
                    SDL_SetRenderDrawColor(renderer, 120, 120, 120, 255);
                    SDL_RenderDrawRect(renderer, &pr);
                }
            }

            // draw pickups (power-ups) and ground items
            // pickups: green; items: cyan; optional AABB debug when overlapping player
            const Entity* pdraw =
                (state.player_vid ? state.entities.get(*state.player_vid) : nullptr);
            glm::vec2 ph{0.0f};
            if (pdraw)
                ph = pdraw->half_size();
            float pl = 0, pr = 0, pt = 0, pb = 0;
            if (pdraw) {
                pl = pdraw->pos.x - ph.x;
                pr = pdraw->pos.x + ph.x;
                pt = pdraw->pos.y - ph.y;
                pb = pdraw->pos.y + ph.y;
            }
            if (state.mode == ids::MODE_PLAYING)
            for (auto const& pu : state.pickups.data())
                if (pu.active) {
                    SDL_FPoint c = world_to_screen(pu.pos.x - 0.125f, pu.pos.y - 0.125f);
                    float s = TILE_SIZE * gfx.play_cam.zoom * 0.20f;
                    SDL_Rect r{(int)std::floor(c.x), (int)std::floor(c.y), (int)std::ceil(s),
                               (int)std::ceil(s)};
                    int sid = pu.sprite_id;
                    if (sid < 0 && g_lua_mgr && g_sprite_ids) {
                        // resolve sprite from powerup def
                        for (auto const& pd : g_lua_mgr->powerups())
                            if ((int)pd.type == (int)pu.type) {
                                if (!pd.sprite.empty() &&
                                    pd.sprite.find(':') != std::string::npos)
                                    sid = g_sprite_ids->try_get(pd.sprite);
                                break;
                            }
                    }
                    if (sid >= 0) {
                        if (SDL_Texture* tex = textures.get(sid))
                            SDL_RenderCopy(renderer, tex, nullptr, &r);
                        else
                            add_warning("Missing texture for powerup sprite");
                    } else {
                        add_warning("Missing sprite for powerup");
                        SDL_SetRenderDrawColor(renderer, 100, 220, 120, 255);
                        SDL_RenderFillRect(renderer, &r);
                    }
                    if (pdraw) {
                        float gl = pu.pos.x - 0.125f, gr = pu.pos.x + 0.125f;
                        float gt = pu.pos.y - 0.125f, gb = pu.pos.y + 0.125f;
                        bool overlap = !(pr <= gl || pl >= gr || pb <= gt || pt >= gb);
                        if (overlap) {
                            SDL_SetRenderDrawColor(renderer, 240, 220, 80, 255);
                            SDL_RenderDrawRect(renderer, &r);
                        }
                    }
                }
            if (state.mode == ids::MODE_PLAYING)
            for (auto const& gi : state.ground_items.data())
                if (gi.active) {
                    SDL_FPoint c =
                        world_to_screen(gi.pos.x - gi.size.x * 0.5f, gi.pos.y - gi.size.y * 0.5f);
                    float scale = TILE_SIZE * gfx.play_cam.zoom;
                    SDL_Rect r{(int)std::floor(c.x), (int)std::floor(c.y),
                               (int)std::ceil(gi.size.x * scale),
                               (int)std::ceil(gi.size.y * scale)};
                    // Resolve sprite id from item def name/sprite
                    int ispr = -1;
                    if (g_lua_mgr) {
                        const ItemInstance* inst = state.items.get(gi.item_vid);
                        if (inst) {
                            for (auto const& d : g_lua_mgr->items())
                                if (d.type == inst->def_type) {
                                    if (!d.sprite.empty() &&
                                        d.sprite.find(':') != std::string::npos) {
                                        ispr = g_sprite_ids ? g_sprite_ids->try_get(d.sprite) : -1;
                                    } else {
                                        ispr = -1;
                                    }
                                    break;
                                }
                        }
                    }
                    if (ispr >= 0) {
                        if (SDL_Texture* tex = textures.get(ispr))
                            SDL_RenderCopy(renderer, tex, nullptr, &r);
                    } else {
                        add_warning("Missing sprite for item");
                        SDL_SetRenderDrawColor(renderer, 80, 220, 240, 255);
                        SDL_RenderFillRect(renderer, &r);
                    }
                    // outline handled in consolidated best-overlap block
                }
            // draw ground guns (magenta if no sprite)
            if (state.mode == ids::MODE_PLAYING)
            for (auto const& gg : state.ground_guns.data())
                if (gg.active) {
                    SDL_FPoint c =
                        world_to_screen(gg.pos.x - gg.size.x * 0.5f, gg.pos.y - gg.size.y * 0.5f);
                    float scale = TILE_SIZE * gfx.play_cam.zoom;
                    SDL_Rect r{(int)std::floor(c.x), (int)std::floor(c.y),
                               (int)std::ceil(gg.size.x * scale),
                               (int)std::ceil(gg.size.y * scale)};
                    int sid = gg.sprite_id;
                    if (sid < 0 && g_lua_mgr && g_sprite_ids) {
                        if (const GunInstance* gi = state.guns.get(gg.gun_vid)) {
                            const GunDef* gd = nullptr;
                            for (auto const& g : g_lua_mgr->guns())
                                if (g.type == gi->def_type) {
                                    gd = &g;
                                    break;
                                }
                            if (gd && !gd->sprite.empty() && gd->sprite.find(':') != std::string::npos)
                                sid = g_sprite_ids->try_get(gd->sprite);
                        }
                    }
                    if (sid >= 0) {
                        if (SDL_Texture* tex = textures.get(sid))
                            SDL_RenderCopy(renderer, tex, nullptr, &r);
                        else
                            add_warning("Missing texture for gun sprite");
                    } else {
                        add_warning("Missing sprite for gun");
                        SDL_SetRenderDrawColor(renderer, 220, 120, 220, 255);
                        SDL_RenderFillRect(renderer, &r);
                    }
                }
            // Consolidated pickup prompt: show only for the most-overlapped target
            if (pdraw && ui_font && state.mode == ids::MODE_PLAYING) {
                enum class PK { None, Gun, Item };
                PK best_kind = PK::None;
                std::size_t best_idx = (std::size_t)-1;
                float best_area = 0.0f;
                auto overlap_area = [&](float al, float at, float ar, float ab, float bl, float bt,
                                       float br, float bb) {
                    float xl = std::max(al, bl);
                    float xr = std::min(ar, br);
                    float yt = std::max(at, bt);
                    float yb = std::min(ab, bb);
                    float w = xr - xl;
                    float h = yb - yt;
                    if (w <= 0.0f || h <= 0.0f)
                        return 0.0f;
                    return w * h;
                };
                for (std::size_t i = 0; i < state.ground_guns.data().size(); ++i) {
                    auto const& gg = state.ground_guns.data()[i];
                    if (!gg.active)
                        continue;
                    glm::vec2 gh = gg.size * 0.5f;
                    float gl = gg.pos.x - gh.x, gr = gg.pos.x + gh.x;
                    float gt = gg.pos.y - gh.y, gb = gg.pos.y + gh.y;
                    float area_g = overlap_area(pl, pt, pr, pb, gl, gt, gr, gb);
                    if (area_g > best_area) {
                        best_area = area_g;
                        best_kind = PK::Gun;
                        best_idx = i;
                    }
                }
                for (std::size_t i = 0; i < state.ground_items.data().size(); ++i) {
                    auto const& gi = state.ground_items.data()[i];
                    if (!gi.active)
                        continue;
                    glm::vec2 gh = gi.size * 0.5f;
                    float gl = gi.pos.x - gh.x, gr = gi.pos.x + gh.x;
                    float gt = gi.pos.y - gh.y, gb = gi.pos.y + gh.y;
                    float area_i = overlap_area(pl, pt, pr, pb, gl, gt, gr, gb);
                    if (area_i > best_area) {
                        best_area = area_i;
                        best_kind = PK::Item;
                        best_idx = i;
                    }
                }
                if (best_area > 0.0f) {
                    std::string nm;
                    SDL_Rect r{0, 0, 0, 0};
                    if (best_kind == PK::Item) {
                        auto const& gi = state.ground_items.data()[best_idx];
                        // screen rect for item
                        SDL_FPoint c = world_to_screen(gi.pos.x - gi.size.x * 0.5f,
                                                       gi.pos.y - gi.size.y * 0.5f);
                        float scale = TILE_SIZE * gfx.play_cam.zoom;
                        r = SDL_Rect{(int)std::floor(c.x), (int)std::floor(c.y),
                                     (int)std::ceil(gi.size.x * scale),
                                     (int)std::ceil(gi.size.y * scale)};
                        nm = "item";
                        if (g_lua_mgr) {
                            const ItemInstance* inst = state.items.get(gi.item_vid);
                            if (inst) {
                                for (auto const& d : g_lua_mgr->items())
                                    if (d.type == inst->def_type) {
                                        nm = d.name;
                                        break;
                                    }
                            }
                        }
                    } else if (best_kind == PK::Gun) {
                        auto const& gg = state.ground_guns.data()[best_idx];
                        SDL_FPoint c = world_to_screen(gg.pos.x - gg.size.x * 0.5f,
                                                       gg.pos.y - gg.size.y * 0.5f);
                        float scale = TILE_SIZE * gfx.play_cam.zoom;
                        r = SDL_Rect{(int)std::floor(c.x), (int)std::floor(c.y),
                                     (int)std::ceil(gg.size.x * scale),
                                     (int)std::ceil(gg.size.y * scale)};
                        nm = "gun";
                        if (g_lua_mgr) {
                            if (const GunInstance* gi = state.guns.get(gg.gun_vid)) {
                                for (auto const& g : g_lua_mgr->guns())
                                    if (g.type == gi->def_type) {
                                        nm = g.name;
                                        break;
                                    }
                            }
                        }
                    }
                    // Outline highlight for the best candidate
                    SDL_SetRenderDrawColor(renderer, 240, 220, 80, 255);
                    SDL_RenderDrawRect(renderer, &r);
                    const char* keyname = SDL_GetScancodeName(binds.pick_up);
                    if (!keyname || !*keyname)
                        keyname = "F";
                    std::string prompt = std::string("Press ") + keyname + " to pick up " + nm;
                    SDL_Color col{250, 250, 250, 255};
                    SDL_Surface* s = TTF_RenderUTF8_Blended(ui_font, prompt.c_str(), col);
                    if (s) {
                        SDL_Texture* t = SDL_CreateTextureFromSurface(renderer, s);
                        int tw = 0, th = 0;
                        SDL_QueryTexture(t, nullptr, nullptr, &tw, &th);
                        SDL_Rect d{r.x, r.y - th - 2, tw, th};
                        SDL_RenderCopy(renderer, t, nullptr, &d);
                        SDL_DestroyTexture(t);
                        SDL_FreeSurface(s);
                    }
                    // Center inspect view for ground target when gun panel toggle (V) is on
                    if (state.show_gun_panel) {
                        int panel_w = (int)std::lround(width * 0.32);
                        int px = (width - panel_w) / 2;
                        int py = (int)std::lround(height * 0.22);
                        SDL_Rect box{px, py, panel_w, 420};
                        SDL_SetRenderDrawColor(renderer, 25, 25, 30, 220);
                        SDL_RenderFillRect(renderer, &box);
                        SDL_SetRenderDrawColor(renderer, 200, 200, 220, 255);
                        SDL_RenderDrawRect(renderer, &box);
                        int tx = px + 12;
                        int ty = py + 12;
                        int lh = 18;
                        // (no extra helpers here)
                        // (helpers removed; use global ui_draw_kv_line/fmt2)
                        if (best_kind == PK::Item) {
                            auto const& gi = state.ground_items.data()[best_idx];
                            const ItemInstance* inst = state.items.get(gi.item_vid);
                            std::string iname = "item";
                            std::string idesc;
                            bool consume = false;
                            int sid = -1;
                            if (g_lua_mgr && inst) {
                                for (auto const& ddf : g_lua_mgr->items())
                                    if (ddf.type == inst->def_type) {
                                        iname = ddf.name;
                                        idesc = ddf.desc;
                                        consume = ddf.consume_on_use;
                                        if (!ddf.sprite.empty() && ddf.sprite.find(':') != std::string::npos && g_sprite_ids)
                                            sid = g_sprite_ids->try_get(ddf.sprite);
                                        break;
                                    }
                            }
                            if (sid >= 0) if (SDL_Texture* texi = textures.get(sid)) { SDL_Rect dst{tx, ty, 48, 32}; SDL_RenderCopy(renderer, texi, nullptr, &dst); ty += 36; }
                            ui_draw_kv_line(renderer, ui_font, tx, ty, lh, "Item", iname);
                            if (!idesc.empty()) ui_draw_kv_line(renderer, ui_font, tx, ty, lh, "Desc", idesc);
                            ui_draw_kv_line(renderer, ui_font, tx, ty, lh, "Consumable", consume ? std::string("Yes") : std::string("No"));
                        } else if (best_kind == PK::Gun) {
                            auto const& gg = state.ground_guns.data()[best_idx];
                            const GunInstance* gim = state.guns.get(gg.gun_vid);
                            const GunDef* gdp = nullptr;
                            if (g_lua_mgr && gim) {
                                for (auto const& gdf : g_lua_mgr->guns()) if (gdf.type == gim->def_type) { gdp = &gdf; break; }
                            }
                            if (gdp) {
                                int gun_sid = -1;
                                if (!gdp->sprite.empty() && g_sprite_ids) gun_sid = g_sprite_ids->try_get(gdp->sprite);
                                if (gun_sid >= 0) if (SDL_Texture* texg = textures.get(gun_sid)) { SDL_Rect dst{tx, ty, 64, 40}; SDL_RenderCopy(renderer, texg, nullptr, &dst); ty += 44; }
                                ui_draw_kv_line(renderer, ui_font, tx, ty, lh, "Gun", gdp->name);
                                ui_draw_kv_line(renderer, ui_font, tx, ty, lh, "Damage", std::to_string((int)std::lround(gdp->damage)));
                                ui_draw_kv_line(renderer, ui_font, tx, ty, lh, "RPM", std::to_string((int)std::lround(gdp->rpm)));
                                ui_draw_kv_line(renderer, ui_font, tx, ty, lh, "Deviation", fmt2(gdp->deviation) + " deg");
                                ui_draw_kv_line(renderer, ui_font, tx, ty, lh, "Pellets", std::to_string(gdp->pellets_per_shot));
                                ui_draw_kv_line(renderer, ui_font, tx, ty, lh, "Recoil", fmt2(gdp->recoil));
                                ui_draw_kv_line(renderer, ui_font, tx, ty, lh, "Control", fmt2(gdp->control));
                                ui_draw_kv_line(renderer, ui_font, tx, ty, lh, "Recoil cap", std::to_string((int)std::lround(gdp->max_recoil_spread_deg)) + " deg");
                                ui_draw_kv_line(renderer, ui_font, tx, ty, lh, "Reload/Eject", std::to_string((int)std::lround(gdp->reload_time * 1000.0f)) + "/" + std::to_string((int)std::lround(gdp->eject_time * 1000.0f)) + " ms");
                                ui_draw_kv_line(renderer, ui_font, tx, ty, lh, "Jam", std::to_string((int)std::lround(gdp->jam_chance * 100.0f)) + " %");
                                ui_draw_kv_line(renderer, ui_font, tx, ty, lh, "AR Center", fmt2(gdp->ar_pos) + " ±" + fmt2(gdp->ar_pos_variance));
                                ui_draw_kv_line(renderer, ui_font, tx, ty, lh, "AR Size", fmt2(gdp->ar_size) + " ±" + fmt2(gdp->ar_size_variance));
                                if (gim && gim->ammo_type != 0) {
                                    if (auto const* ad = g_lua_mgr->find_ammo(gim->ammo_type)) {
                                        int asid = (!ad->sprite.empty() && g_sprite_ids) ? g_sprite_ids->try_get(ad->sprite) : -1;
                                        if (asid >= 0) if (SDL_Texture* texa = textures.get(asid)) { SDL_Rect dst{tx, ty, 36, 20}; SDL_RenderCopy(renderer, texa, nullptr, &dst); ty += 22; }
                                        ui_draw_kv_line(renderer, ui_font, tx, ty, lh, "Ammo", ad->name);
                                        if (!ad->desc.empty()) ui_draw_kv_line(renderer, ui_font, tx, ty, lh, "Desc", ad->desc);
                                        int apct = (int)std::lround(ad->armor_pen * 100.0f);
                                        int save_tx2 = tx; tx += 10;
                                        ui_draw_kv_line(renderer, ui_font, tx, ty, lh, "DMG", fmt2(ad->damage_mult));
                                        ui_draw_kv_line(renderer, ui_font, tx, ty, lh, "AP", std::to_string(apct) + "%");
                                        ui_draw_kv_line(renderer, ui_font, tx, ty, lh, "Shield", fmt2(ad->shield_mult));
                                        if (ad->range_units > 0.0f) {
                                            ui_draw_kv_line(renderer, ui_font, tx, ty, lh, "Range", std::to_string((int)std::lround(ad->range_units)));
                                            ui_draw_kv_line(renderer, ui_font, tx, ty, lh, "Falloff", std::to_string((int)std::lround(ad->falloff_start)) + "→" + std::to_string((int)std::lround(ad->falloff_end)));
                                            ui_draw_kv_line(renderer, ui_font, tx, ty, lh, "Min Mult", fmt2(ad->falloff_min_mult));
                                        }
                                        ui_draw_kv_line(renderer, ui_font, tx, ty, lh, "Speed", std::to_string((int)std::lround(ad->speed)));
                                        ui_draw_kv_line(renderer, ui_font, tx, ty, lh, "Pierce", std::to_string(ad->pierce_count));
                                        tx = save_tx2;
                                    }
                                }
                            }
                        }
                    }
                }
            }
            // draw projectiles (prefer sprite; fallback to red rect)
            for (auto const& proj : projectiles.items)
                if (proj.active) {
                    SDL_FPoint c = world_to_screen(proj.pos.x - proj.size.x * 0.5f,
                                                   proj.pos.y - proj.size.y * 0.5f);
                    float scale = TILE_SIZE * gfx.play_cam.zoom;
                    SDL_Rect r{(int)std::floor(c.x), (int)std::floor(c.y),
                               (int)std::ceil(proj.size.x * scale),
                               (int)std::ceil(proj.size.y * scale)};
                    bool drew = false;
                    if (proj.sprite_id >= 0) {
                        if (SDL_Texture* tex = textures.get(proj.sprite_id)) {
                            SDL_RenderCopy(renderer, tex, nullptr, &r);
                            drew = true;
                        } else {
                            add_warning("Missing texture for projectile sprite");
                        }
                    }
                    if (!drew) {
                        add_warning("Missing sprite for projectile");
                        SDL_SetRenderDrawColor(renderer, 240, 80, 80, 255);
                        SDL_RenderFillRect(renderer, &r);
                    }
                }

            // (alerts are rendered at the very end for top-layer visibility)

            // draw cursor crosshair + circle (in screen-space at actual mouse position)
            if (state.mode == ids::MODE_PLAYING) {
                int mx = state.mouse_inputs.pos.x;
                int my = state.mouse_inputs.pos.y;
                if (state.reticle_shake > 0.01f) {
                    static thread_local std::mt19937 rng{std::random_device{}()};
                    std::uniform_real_distribution<float> J(-state.reticle_shake,
                                                            state.reticle_shake);
                    mx += (int)std::lround(J(rng));
                    my += (int)std::lround(J(rng));
                    state.reticle_shake *= 0.90f;
                } else {
                    state.reticle_shake = 0.0f;
                }
                SDL_SetRenderDrawColor(renderer, 250, 250, 250, 220);
                int cross = 8;
                SDL_RenderDrawLine(renderer, mx - cross, my, mx + cross, my);
                SDL_RenderDrawLine(renderer, mx, my - cross, mx, my + cross);
                // circle approximation
                // Compute accuracy-based reticle radius to match angular deviation
                float reticle_radius_px = 12.0f;
                // If player has gun, compute angular spread
                if (state.player_vid) {
                    const Entity* plv = state.entities.get(*state.player_vid);
                    if (plv && plv->equipped_gun_vid.has_value() && g_lua_mgr) {
                        const GunInstance* gi = state.guns.get(*plv->equipped_gun_vid);
                        const GunDef* gd = nullptr;
                        if (gi) {
                            for (auto const& g : g_lua_mgr->guns())
                                if (g.type == gi->def_type) { gd = &g; break; }
                        }
                        if (gd) {
                            // Effective base deviation scaled by player accuracy
                            float acc = std::max(0.1f, plv->stats.accuracy / 100.0f);
                            float base_dev = gd->deviation / acc;
                            // Movement spread from per-entity accumulator
                            float move_spread = plv->move_spread_deg / acc;
                            float recoil_spread = gi->spread_recoil_deg;
                            float theta_deg = std::clamp(base_dev + move_spread + recoil_spread,
                                                         MIN_SPREAD_DEG, MAX_SPREAD_DEG);
                            // Convert to positional radius at current cursor distance
                            // Compute player->mouse world distance
                            int ww = width, wh = height;
                            SDL_GetRendererOutputSize(renderer, &ww, &wh);
                            float inv_scale = 1.0f / (TILE_SIZE * gfx.play_cam.zoom);
                            // World positions
                            glm::vec2 ppos = plv->pos;
                            glm::vec2 mpos = {gfx.play_cam.pos.x + (static_cast<float>(state.mouse_inputs.pos.x) -
                                                         static_cast<float>(ww) * 0.5f) * inv_scale,
                                              gfx.play_cam.pos.y + (static_cast<float>(state.mouse_inputs.pos.y) -
                                                         static_cast<float>(wh) * 0.5f) * inv_scale};
                            float dist = std::sqrt((mpos.x - ppos.x)*(mpos.x - ppos.x) + (mpos.y - ppos.y)*(mpos.y - ppos.y));
                            float theta_rad = theta_deg * 3.14159265358979323846f / 180.0f;
                            float r_world = dist * std::tan(theta_rad);
                            reticle_radius_px = std::max(6.0f, r_world * TILE_SIZE * gfx.play_cam.zoom);
                        }
                    }
                }
                int radius = (int)std::lround(reticle_radius_px);
                const int segments = 32;
                float prevx = static_cast<float>(mx) + static_cast<float>(radius);
                float prevy = (float)my;
                for (int i = 1; i <= segments; ++i) {
                    float ang = static_cast<float>(i) *
                                (2.0f * 3.14159265358979323846f / static_cast<float>(segments));
                    float x = static_cast<float>(mx) + std::cos(ang) * static_cast<float>(radius);
                    float y = static_cast<float>(my) + std::sin(ang) * static_cast<float>(radius);
                    SDL_RenderDrawLine(renderer, (int)prevx, (int)prevy, (int)x, (int)y);
                    prevx = x;
                    prevy = y;
                }
                // If gun equipped, draw vertical mag + reserve bars next to reticle
                if (state.player_vid) {
                    const Entity* plv = state.entities.get(*state.player_vid);
                    if (plv && plv->equipped_gun_vid.has_value() && g_lua_mgr) {
                        const GunInstance* gi = state.guns.get(*plv->equipped_gun_vid);
                        if (gi) {
                            const GunDef* gd = nullptr;
                            for (auto const& g : g_lua_mgr->guns())
                                if (g.type == gi->def_type) {
                                    gd = &g;
                                    break;
                                }
                            if (gd) {
                                int bar_h = 60;
                                int bar_w = 8;
                                int gap = 2;
                                int rx = mx + 16;
                                int ry = my - bar_h / 2;
                                if (state.reload_bar_shake > 0.01f) {
                                    static thread_local std::mt19937 rng{std::random_device{}()};
                                    std::uniform_real_distribution<float> J(-state.reload_bar_shake,
                                                                            state.reload_bar_shake);
                                    rx += (int)std::lround(J(rng));
                                    ry += (int)std::lround(J(rng));
                                    state.reload_bar_shake *= 0.90f;
                                } else {
                                    state.reload_bar_shake = 0.0f;
                                }
                                float mag_ratio =
                                    (gd->mag > 0) ? (float)gi->current_mag / (float)gd->mag : 0.0f;
                                float res_ratio = (gd->ammo_max > 0) ? (float)gi->ammo_reserve /
                                                                           (float)gd->ammo_max
                                                                     : 0.0f;
                                // no tilt rendering here
                                bool reloading =
                                    (gi->reloading || gi->reload_eject_remaining > 0.0f);
                                if (!reloading) {
                                    // mag bar
                                    SDL_Rect bg{rx, ry, bar_w, bar_h};
                                    SDL_SetRenderDrawColor(renderer, 40, 40, 50, 180);
                                    SDL_RenderFillRect(renderer, &bg);
                                    int fill_h =
                                        (int)std::lround((double)bar_h * (double)mag_ratio);
                                    SDL_Rect fg{rx, ry + (bar_h - fill_h), bar_w, fill_h};
                                    SDL_SetRenderDrawColor(renderer, 200, 240, 255, 220);
                                    SDL_RenderFillRect(renderer, &fg);
                                    // reserve sliver
                                    SDL_Rect bg2{rx + bar_w + gap, ry, 3, bar_h};
                                    SDL_SetRenderDrawColor(renderer, 40, 40, 50, 180);
                                    SDL_RenderFillRect(renderer, &bg2);
                                    int rfill = (int)std::lround((double)bar_h * (double)res_ratio);
                                    SDL_Rect rf{rx + bar_w + gap, ry + (bar_h - rfill), 3, rfill};
                                    SDL_SetRenderDrawColor(renderer, 180, 200, 200, 220);
                                    SDL_RenderFillRect(renderer, &rf);
                                } else {
                                    // non-tilted reload UI with growing progress rectangle
                                    // mag background
                                    SDL_Rect bg{rx, ry, bar_w, bar_h};
                                    SDL_SetRenderDrawColor(renderer, 40, 40, 50, 180);
                                    SDL_RenderFillRect(renderer, &bg);
                                    // active window overlay
                                    if (gi->reload_total_time > 0.0f) {
                                        float ws = gi->ar_window_start, we = gi->ar_window_end;
                                        ws = std::clamp(ws, 0.0f, 1.0f);
                                        we = std::clamp(we, 0.0f, 1.0f);
                                        int wy0 = ry + (int)std::lround((double)bar_h *
                                                                        (double)(1.0f - we));
                                        int wy1 = ry + (int)std::lround((double)bar_h *
                                                                        (double)(1.0f - ws));
                                        SDL_Rect win{rx - 2, wy0, bar_w + 6,
                                                     std::max(2, wy1 - wy0)};
                                        bool lockout = (gi->ar_consumed && gi->ar_failed_attempt);
                                        if (lockout)
                                            SDL_SetRenderDrawColor(renderer, 120, 120, 120, 140);
                                        else
                                            SDL_SetRenderDrawColor(renderer, 240, 220, 80, 120);
                                        SDL_RenderFillRect(renderer, &win);
                                        // progress rectangle grows from bottom of bar
                                        int prg_h = (int)std::lround((double)bar_h *
                                                                     (double)gi->reload_progress);
                                        if (prg_h > 0) {
                                            SDL_Rect prog{rx - 2, ry + (bar_h - prg_h), bar_w + 6,
                                                          prg_h};
                                            if (lockout)
                                                SDL_SetRenderDrawColor(renderer, 110, 110, 120,
                                                                       220);
                                            else
                                                SDL_SetRenderDrawColor(renderer, 200, 240, 255,
                                                                       200);
                                            SDL_RenderFillRect(renderer, &prog);
                                        }
                                    }
                                    // reserve sliver (unchanged)
                                    SDL_Rect bg2{rx + bar_w + gap, ry, 3, bar_h};
                                    SDL_SetRenderDrawColor(renderer, 40, 40, 50, 180);
                                    SDL_RenderFillRect(renderer, &bg2);
                                    int rfill = (int)std::lround((double)bar_h * (double)res_ratio);
                                    SDL_Rect rf{rx + bar_w + gap, ry + (bar_h - rfill), 3, rfill};
                                    SDL_SetRenderDrawColor(renderer, 180, 200, 200, 220);
                                    SDL_RenderFillRect(renderer, &rf);
                                }
                                // Text label priority: JAMMED! > RELOAD/NO AMMO
                                if (ui_font) {
                                    const char* txt = nullptr;
                                    SDL_Color col{250, 220, 80, 255};
                                    if (gi->jammed) {
                                        txt = "JAMMED!";
                                        col = SDL_Color{240, 80, 80, 255};
                                    } else if (gi->current_mag == 0) {
                                        txt = (gi->ammo_reserve > 0) ? "RELOAD" : "NO AMMO";
                                        col = SDL_Color{250, 220, 80, 255};
                                    }
                                    if (txt) {
                                        SDL_Surface* s = TTF_RenderUTF8_Blended(ui_font, txt, col);
                                        if (s) {
                                            SDL_Texture* t = SDL_CreateTextureFromSurface(renderer, s);
                                            int tw = 0, th = 0;
                                            SDL_QueryTexture(t, nullptr, nullptr, &tw, &th);
                                            SDL_Rect d{rx - 4, ry - th - 4, tw, th};
                                            SDL_RenderCopy(renderer, t, nullptr, &d);
                                            SDL_DestroyTexture(t);
                                            SDL_FreeSurface(s);
                                        }
                                    }
                                }
                                // jam UI: draw unjam progress when jammed
                                if (gi->jammed) {
                                    SDL_Rect jb{rx - 12, ry, 4, bar_h};
                                    SDL_SetRenderDrawColor(renderer, 50, 30, 30, 200);
                                    SDL_RenderFillRect(renderer, &jb);
                                    int jh = (int)std::lround((double)bar_h *
                                                              (double)gi->unjam_progress);
                                    SDL_Rect jf{rx - 12, ry + (bar_h - jh), 4, jh};
                                    SDL_SetRenderDrawColor(renderer, 240, 60, 60, 240);
                                    SDL_RenderFillRect(renderer, &jf);
                                }
                            }
                        }
                    }
                }
            }
            // Countdown overlay (shifted down, with label)
            if (state.mode == ids::MODE_PLAYING && state.exit_countdown >= 0.0f) {
                float ratio = state.exit_countdown / EXIT_COUNTDOWN_SECONDS;
                ratio = std::clamp(ratio, 0.0f, 1.0f);
                int bar_w = width - 40;
                int bar_h = 12;
                int bar_x = 20;
                int bar_y = 48; // shifted down a bit
                SDL_Rect bg{bar_x, bar_y, bar_w, bar_h};
                SDL_SetRenderDrawColor(renderer, 40, 40, 40, 200);
                SDL_RenderFillRect(renderer, &bg);
                int fgw = (int)std::lround(static_cast<double>(bar_w) *
                                           static_cast<double>(1.0f - ratio));
                SDL_Rect fg{bar_x, bar_y, fgw, bar_h};
                SDL_SetRenderDrawColor(renderer, 240, 220, 80, 220);
                SDL_RenderFillRect(renderer, &fg);
                // Outline
                SDL_SetRenderDrawColor(renderer, 10, 10, 10, 255);
                SDL_RenderDrawRect(renderer, &bg);

                // Label + Numeric overlay
                if (ui_font) {
                    // Label
                    const char* label = "Exiting to next area";
                    SDL_Color lc{240, 220, 80, 255};
                    SDL_Surface* lsurf = TTF_RenderUTF8_Blended(ui_font, label, lc);
                    if (lsurf) {
                        SDL_Texture* ltex = SDL_CreateTextureFromSurface(renderer, lsurf);
                        if (ltex) {
                            int lw = 0, lh = 0;
                            SDL_QueryTexture(ltex, nullptr, nullptr, &lw, &lh);
                            SDL_Rect ldst{bar_x, bar_y - lh - 6, lw, lh};
                            SDL_RenderCopy(renderer, ltex, nullptr, &ldst);
                            SDL_DestroyTexture(ltex);
                        }
                        SDL_FreeSurface(lsurf);
                    }
                    char txt[16];
                    float secs = std::max(0.0f, state.exit_countdown);
                    std::snprintf(txt, sizeof(txt), "%.1f", (double)secs);
                    SDL_Color color{255, 255, 255, 255};
                    SDL_Surface* surf = TTF_RenderUTF8_Blended(ui_font, txt, color);
                    if (surf) {
                        SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surf);
                        if (tex) {
                            int tw = 0, th = 0;
                            SDL_QueryTexture(tex, nullptr, nullptr, &tw, &th);
                            SDL_Rect dst{bar_x + bar_w / 2 - tw / 2, bar_y - th - 4, tw, th};
                            SDL_RenderCopy(renderer, tex, nullptr, &dst);
                            SDL_DestroyTexture(tex);
                        }
                        SDL_FreeSurface(surf);
                    }
                }
            }

            // Score review page (full-screen overlay)
            if (state.mode == ids::MODE_SCORE_REVIEW) {
                // Full-screen backdrop
                SDL_Rect full{0, 0, width, height};
                SDL_SetRenderDrawColor(renderer, 18, 18, 22, 255);
                SDL_RenderFillRect(renderer, &full);
                // Heading and prompt
                if (ui_font) {
                    SDL_Color titlec{240, 220, 80, 255};
                    SDL_Surface* ts = TTF_RenderUTF8_Blended(ui_font, "Stage Clear", titlec);
                    if (ts) {
                        SDL_Texture* tt = SDL_CreateTextureFromSurface(renderer, ts);
                        int tw=0, th=0; SDL_QueryTexture(tt, nullptr, nullptr, &tw, &th);
                        SDL_Rect td{40, 40, tw, th};
                        SDL_RenderCopy(renderer, tt, nullptr, &td);
                        SDL_DestroyTexture(tt);
                        SDL_FreeSurface(ts);
                    }
                    if (state.score_ready_timer <= 0.0f) {
                        SDL_Color pc{200, 200, 210, 255};
                        SDL_Surface* ps = TTF_RenderUTF8_Blended(ui_font,
                            "Press SPACE or CLICK to continue", pc);
                        if (ps) {
                            SDL_Texture* ptex = SDL_CreateTextureFromSurface(renderer, ps);
                            int prompt_w=0, prompt_h=0; SDL_QueryTexture(ptex, nullptr, nullptr, &prompt_w, &prompt_h);
                            SDL_Rect pd{width/2 - prompt_w/2, height - prompt_h - 20, prompt_w, prompt_h};
                            SDL_RenderCopy(renderer, ptex, nullptr, &pd);
                            SDL_DestroyTexture(ptex);
                            SDL_FreeSurface(ps);
                        }
                    }
                }
                // Animation: reveal one stat every 0.2s; tick numbers at 20Hz
                if (state.score_ready_timer <= 0.0f) {
                    state.review_next_stat_timer -= TIMESTEP;
                    if (state.review_next_stat_timer <= 0.0f && state.review_revealed < state.review_stats.size()) {
                        state.review_next_stat_timer = 0.2f;
                        state.review_revealed += 1; // reveal next
                        sounds.play("base:small_shoot");
                    }
                    // number ticking
                    state.review_number_tick_timer += TIMESTEP;
                    while (state.review_number_tick_timer >= 0.05f) {
                        state.review_number_tick_timer -= 0.05f;
                        for (std::size_t i = 0; i < state.review_revealed; ++i) {
                            auto& rs = state.review_stats[i];
                            if (rs.header || rs.done)
                                continue;
                            double step = std::max(1.0, std::floor(rs.target / 20.0));
                            // For fractional targets (time/accuracy/distance), use 1/20th per tick
                            if (rs.target < 20.0)
                                step = std::max(0.1, rs.target / 20.0);
                            rs.value = std::min(rs.target, rs.value + step);
                            sounds.play("base:small_shoot");
                            if (rs.value >= rs.target)
                                rs.done = true;
                        }
                    }
                }
                // Text metrics (animated, one per line)
                if (ui_font) {
                    int tx = 40;
                    int ty = 80;
                    auto draw_line = [&](const std::string& s, SDL_Color col) {
                        SDL_Surface* srf = TTF_RenderUTF8_Blended(ui_font, s.c_str(), col);
                        if (!srf)
                            return;
                        SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, srf);
                        int tw=0, th=0; SDL_QueryTexture(tex, nullptr, nullptr, &tw, &th);
                        SDL_Rect d{tx, ty, tw, th};
                        SDL_RenderCopy(renderer, tex, nullptr, &d);
                        SDL_DestroyTexture(tex);
                        SDL_FreeSurface(srf);
                        ty += 18;
                    };
                    SDL_Color mc{210, 210, 220, 255};
                    char buf[128];
                    for (std::size_t i = 0; i < state.review_revealed && i < state.review_stats.size(); ++i) {
                        auto const& rs = state.review_stats[i];
                        if (rs.header) {
                            draw_line(rs.label, SDL_Color{240, 220, 80, 255});
                        } else {
                            // format number: use integer if near int, else 1 decimal
                            if (std::fabs(rs.target - std::round(rs.target)) < 0.001) {
                                std::snprintf(buf, sizeof(buf), "%s: %d", rs.label.c_str(), (int)std::lround(rs.value));
                            } else {
                                std::snprintf(buf, sizeof(buf), "%s: %.1f", rs.label.c_str(), rs.value);
                            }
                            draw_line(buf, mc);
                        }
                    }
                }
                // Ready indicator bar if waiting
                if (state.score_ready_timer > 0.0f) {
                    float ratio = state.score_ready_timer / SCORE_REVIEW_INPUT_DELAY;
                    ratio = std::clamp(ratio, 0.0f, 1.0f);
                    int wbw = (int)std::lround(static_cast<double>(width - 80) * static_cast<double>(ratio));
                    SDL_Rect waitbar{40, height - 80, wbw, 8};
                    SDL_SetRenderDrawColor(renderer, 240, 220, 80, 220);
                    SDL_RenderFillRect(renderer, &waitbar);
                }
            }

            // Next stage details page (full-screen overlay)
            if (state.mode == ids::MODE_NEXT_STAGE) {
                SDL_Rect full{0, 0, width, height};
                SDL_SetRenderDrawColor(renderer, 18, 18, 22, 255);
                SDL_RenderFillRect(renderer, &full);
                int box_w = width - 200;
                int box_h = 140;
                int box_x = (width - box_w) / 2;
                int box_y = 40;
                SDL_Rect box{box_x, box_y, box_w, box_h};
                SDL_SetRenderDrawColor(renderer, 30, 30, 40, 220);
                SDL_RenderFillRect(renderer, &box);
                SDL_SetRenderDrawColor(renderer, 200, 200, 220, 255);
                SDL_RenderDrawRect(renderer, &box);
                // Simple decorative line as header separator
                SDL_SetRenderDrawColor(renderer, 240, 220, 80, 255);
                SDL_RenderDrawLine(renderer, box_x + 20, box_y + 20, box_x + box_w - 20,
                                   box_y + 20);
                // Heading and prompt
                if (ui_font) {
                    SDL_Color titlec{240, 220, 80, 255};
                    SDL_Surface* ts = TTF_RenderUTF8_Blended(ui_font, "Next Area", titlec);
                    if (ts) {
                        SDL_Texture* tt = SDL_CreateTextureFromSurface(renderer, ts);
                        int tw=0, th=0; SDL_QueryTexture(tt, nullptr, nullptr, &tw, &th);
                        SDL_Rect td{box_x + 24, box_y + 16, tw, th};
                        SDL_RenderCopy(renderer, tt, nullptr, &td);
                        SDL_DestroyTexture(tt);
                        SDL_FreeSurface(ts);
                    }
                    if (state.score_ready_timer <= 0.0f) {
                        SDL_Color pc{200, 200, 210, 255};
                        SDL_Surface* ps = TTF_RenderUTF8_Blended(ui_font,
                            "Press SPACE or CLICK to continue", pc);
                        if (ps) {
                            SDL_Texture* ptex = SDL_CreateTextureFromSurface(renderer, ps);
                            int prompt_w=0, prompt_h=0; SDL_QueryTexture(ptex, nullptr, nullptr, &prompt_w, &prompt_h);
                            SDL_Rect pd{width/2 - prompt_w/2, height - prompt_h - 20, prompt_w, prompt_h};
                            SDL_RenderCopy(renderer, ptex, nullptr, &pd);
                            SDL_DestroyTexture(ptex);
                            SDL_FreeSurface(ps);
                        }
                    }
                }
            }

            // Character stats panel (left 30%, sliding)
            if (ui_font && state.mode == ids::MODE_PLAYING) {
                float target = state.show_character_panel ? 1.0f : 0.0f;
                // animate
                state.character_panel_slide = state.character_panel_slide +
                                              (target - state.character_panel_slide) *
                                                  std::clamp((float)(dt_sec * 10.0), 0.0f, 1.0f);
                float panel_pct = 0.30f;
                int panel_w =
                    (int)std::lround(static_cast<double>(width) * static_cast<double>(panel_pct));
                int px = (int)std::lround(-static_cast<double>(panel_w) *
                                          static_cast<double>(1.0f - state.character_panel_slide));
                SDL_Rect panel{px, 0, panel_w, height};
                SDL_SetRenderDrawColor(renderer, 15, 15, 20, 220);
                SDL_RenderFillRect(renderer, &panel);
                SDL_SetRenderDrawColor(renderer, 240, 220, 80, 255);
                SDL_RenderDrawRect(renderer, &panel);
                // Stats text
                const Entity* p =
                    (state.player_vid ? state.entities.get(*state.player_vid) : nullptr);
                if (p) {
                    int tx = px + 16;
                    int ty = 24;
                    int lh = 18;
                    auto draw_line = [&](const char* key, float val, const char* suffix) {
                        char buf[128];
                        std::snprintf(buf, sizeof(buf), "%s: %.1f%s", key, (double)val,
                                      suffix ? suffix : "");
                        SDL_Color c{220, 220, 220, 255};
                        SDL_Surface* s = TTF_RenderUTF8_Blended(ui_font, buf, c);
                        if (s) {
                            SDL_Texture* t = SDL_CreateTextureFromSurface(renderer, s);
                            int tw = 0, th = 0;
                            SDL_QueryTexture(t, nullptr, nullptr, &tw, &th);
                            SDL_Rect d{tx, ty, tw, th};
                            SDL_RenderCopy(renderer, t, nullptr, &d);
                            SDL_DestroyTexture(t);
                            SDL_FreeSurface(s);
                        }
                        ty += lh;
                    };
                    draw_line("Health Max", p->stats.max_health, "");
                    draw_line("Health Regen", p->stats.health_regen, "/s");
                    draw_line("Shield Max", p->stats.shield_max, "");
                    draw_line("Shield Regen", p->stats.shield_regen, "/s");
                    draw_line("Armor", p->stats.armor, "%");
                    draw_line("Move Speed", p->stats.move_speed, "/s");
                    draw_line("Dodge", p->stats.dodge, "%");
                    draw_line("Scavenging", p->stats.scavenging, "");
                    draw_line("Currency", p->stats.currency, "");
                    draw_line("Ammo Gain", p->stats.ammo_gain, "");
                    draw_line("Luck", p->stats.luck, "");
                    draw_line("Crit Chance", p->stats.crit_chance, "%");
                    draw_line("Crit Damage", p->stats.crit_damage, "%");
                    draw_line("Headshot Damage", p->stats.headshot_damage, "%");
                    draw_line("Damage Absorb", p->stats.damage_absorb, "");
                    draw_line("Damage Output", p->stats.damage_output, "");
                    draw_line("Healing", p->stats.healing, "");
                    draw_line("Accuracy", p->stats.accuracy, "");
                    draw_line("Terror Level", p->stats.terror_level, "");
                }
            }

            // (alerts moved to top-layer rendering at end of frame)

            // Inventory list (left column under alerts) – gameplay only, hidden if character panel shown
            if (ui_font && state.mode == ids::MODE_PLAYING && !state.show_character_panel) {
                int sx = 40;
                int sy = 140;
                int slot_h = 26;
                int slot_w = 220;
                struct HoverSlot { SDL_Rect r; std::size_t index; };
                std::vector<HoverSlot> inv_hover_rects;
                // Draw 10 slots
                for (int i = 0; i < 10; ++i) {
                    bool selected = (state.inventory.selected_index == (std::size_t)i);
                    SDL_Rect slot{sx, sy + i * slot_h, slot_w, slot_h - 6};
                    SDL_SetRenderDrawColor(renderer, selected ? 30 : 18, selected ? 30 : 18,
                                           selected ? 40 : 22, 200);
                    SDL_RenderFillRect(renderer, &slot);
                    SDL_SetRenderDrawColor(renderer, 60, 60, 70, 255);
                    SDL_RenderDrawRect(renderer, &slot);
                    // hotkey label
                    char hk[4];
                    std::snprintf(hk, sizeof(hk), "%d", (i == 9) ? 0 : (i + 1));
                    SDL_Color hotc{150, 150, 150, 220};
                    SDL_Surface* hs = TTF_RenderUTF8_Blended(ui_font, hk, hotc);
                    if (hs) {
                        SDL_Texture* ht = SDL_CreateTextureFromSurface(renderer, hs);
                        int tw = 0, th = 0;
                        SDL_QueryTexture(ht, nullptr, nullptr, &tw, &th);
                        SDL_Rect d{slot.x - 20, slot.y + 2, tw, th};
                        SDL_RenderCopy(renderer, ht, nullptr, &d);
                        SDL_DestroyTexture(ht);
                        SDL_FreeSurface(hs);
                    }
                    // entry text + icon
                    const InvEntry* ent = state.inventory.get((std::size_t)i);
                    if (ent) {
                        inv_hover_rects.push_back(HoverSlot{slot, (std::size_t)i});
                        int icon_w = slot_h - 8;
                        int icon_h = slot_h - 8;
                        int icon_x = slot.x + 6;
                        int icon_y = slot.y + 3;
                        int label_x = slot.x + 8;
                        std::string label;
                        int label_offset = 0;
                        auto draw_icon = [&](int sprite_id) {
                            if (sprite_id >= 0) {
                                SDL_Texture* tex = textures.get(sprite_id);
                                if (tex) {
                                    SDL_Rect dst{icon_x, icon_y, icon_w, icon_h};
                                    SDL_RenderCopy(renderer, tex, nullptr, &dst);
                                    label_offset = icon_w + 10;
                                }
                            }
                        };
                        if (ent->kind == INV_ITEM) {
                            if (const ItemInstance* inst = state.items.get(ent->vid)) {
                                std::string nm = "item";
                                uint32_t count = inst->count;
                                int sid = -1;
                                if (g_lua_mgr) {
                                    for (auto const& d : g_lua_mgr->items())
                                        if (d.type == inst->def_type) {
                                            nm = d.name;
                                            if (!d.sprite.empty() && d.sprite.find(':') != std::string::npos && g_sprite_ids)
                                                sid = g_sprite_ids->try_get(d.sprite);
                                            break;
                                        }
                                }
                                draw_icon(sid);
                                label = nm;
                                if (count > 1)
                                    label += std::string(" x") + std::to_string(count);
                            }
                        } else if (ent->kind == INV_GUN) {
                            if (const GunInstance* gi = state.guns.get(ent->vid)) {
                                std::string nm = "gun";
                                int sid = -1;
                                if (g_lua_mgr) {
                                    for (auto const& g : g_lua_mgr->guns())
                                        if (g.type == gi->def_type) {
                                            nm = g.name;
                                            if (!g.sprite.empty() && g.sprite.find(':') != std::string::npos && g_sprite_ids)
                                                sid = g_sprite_ids->try_get(g.sprite);
                                            break;
                                        }
                                }
                                draw_icon(sid);
                                label = nm;
                            }
                        }
                        if (!label.empty()) {
                            SDL_Color tc{230, 230, 230, 255};
                            SDL_Surface* ts = TTF_RenderUTF8_Blended(ui_font, label.c_str(), tc);
                            if (ts) {
                                SDL_Texture* tt = SDL_CreateTextureFromSurface(renderer, ts);
                                int tw = 0, th = 0;
                                SDL_QueryTexture(tt, nullptr, nullptr, &tw, &th);
                                SDL_Rect d{label_x + label_offset, slot.y + 2, tw, th};
                                SDL_RenderCopy(renderer, tt, nullptr, &d);
                                SDL_DestroyTexture(tt);
                                SDL_FreeSurface(ts);
                            }
                        }
                    }
                }
                // Drop mode hint
                if (state.drop_mode) {
                    SDL_Color hintc{230, 220, 80, 255};
                    const char* hint = "Drop mode: press 1–0";
                    SDL_Surface* hs = TTF_RenderUTF8_Blended(ui_font, hint, hintc);
                    if (hs) {
                        SDL_Texture* ht = SDL_CreateTextureFromSurface(renderer, hs);
                        int tw = 0, th = 0;
                        SDL_QueryTexture(ht, nullptr, nullptr, &tw, &th);
                        SDL_Rect d{sx, sy - th - 8, tw, th};
                        SDL_RenderCopy(renderer, ht, nullptr, &d);
                        SDL_DestroyTexture(ht);
                        SDL_FreeSurface(hs);
                    }
                }
                // Hover info panel (center) if mouse over a slot with content
                SDL_Point mp{state.mouse_inputs.pos.x, state.mouse_inputs.pos.y};
                std::optional<std::size_t> hover_index{};
                for (auto const& hs : inv_hover_rects) {
                    if (mp.x >= hs.r.x && mp.x <= (hs.r.x + hs.r.w) && mp.y >= hs.r.y && mp.y <= (hs.r.y + hs.r.h)) {
                        hover_index = hs.index;
                        break;
                    }
                }
                // Hover delay (fast)
                if (hover_index.has_value()) {
                    if (state.inv_hover_index == (int)*hover_index) state.inv_hover_time += (float)dt_sec; else { state.inv_hover_index = (int)*hover_index; state.inv_hover_time = 0.0f; }
                } else {
                    state.inv_hover_index = -1;
                    state.inv_hover_time = 0.0f;
                }
                const float HOVER_DELAY = 0.12f;
                // Drag-and-drop handling for inventory slots
                static bool prev_left = false;
                bool now_left = state.mouse_inputs.left;
                if (now_left && !prev_left) {
                    if (hover_index.has_value()) {
                        if (state.inventory.get(*hover_index) != nullptr) {
                            state.inv_dragging = true;
                            state.inv_drag_src = (int)*hover_index;
                        }
                    }
                }
                if (!now_left && prev_left) {
                    if (state.inv_dragging) {
                        if (hover_index.has_value()) {
                            std::size_t dst = *hover_index;
                            std::size_t src = (std::size_t)std::max(0, state.inv_drag_src);
                            if (dst != src) {
                                InvEntry* src_e = state.inventory.get_mut(src);
                                InvEntry* dst_e = state.inventory.get_mut(dst);
                                if (src_e && dst_e) {
                                    std::size_t tmp = src_e->index;
                                    src_e->index = dst_e->index;
                                    dst_e->index = tmp;
                                } else if (src_e && !dst_e) {
                                    src_e->index = dst;
                                }
                                std::sort(state.inventory.entries.begin(), state.inventory.entries.end(),
                                          [](auto const& lhs, auto const& rhs) { return lhs.index < rhs.index; });
                            }
                        }
                        state.inv_dragging = false;
                        state.inv_drag_src = -1;
                    }
                }
                prev_left = now_left;
                if (hover_index.has_value() && state.inv_hover_time >= HOVER_DELAY) {
                    const InvEntry* sel = state.inventory.get(*hover_index);
                    if (sel) {
                        int panel_w = (int)std::lround(width * 0.32);
                        int px = (width - panel_w) / 2;
                        int py = (int)std::lround(height * 0.22);
                        SDL_Rect box{px, py, panel_w, 420};
                        SDL_SetRenderDrawColor(renderer, 25, 25, 30, 220);
                        SDL_RenderFillRect(renderer, &box);
                        SDL_SetRenderDrawColor(renderer, 200, 200, 220, 255);
                        SDL_RenderDrawRect(renderer, &box);
                        int tx = px + 12;
                        int ty = py + 12;
                        int lh = 18;
                        auto draw_txt = [&](const std::string& s, SDL_Color col) {
                            SDL_Surface* srf = TTF_RenderUTF8_Blended(ui_font, s.c_str(), col);
                            if (srf) {
                                SDL_Texture* t = SDL_CreateTextureFromSurface(renderer, srf);
                                int tw = 0, th = 0;
                                SDL_QueryTexture(t, nullptr, nullptr, &tw, &th);
                                SDL_Rect d{tx, ty, tw, th};
                                SDL_RenderCopy(renderer, t, nullptr, &d);
                                SDL_DestroyTexture(t);
                                SDL_FreeSurface(srf);
                            }
                            ty += lh;
                        };
                        // (no local helpers here)
                        // (hover selected item panel keeps simple draw_txt; no kv helpers here to avoid unused warnings)
                        if (sel->kind == INV_ITEM) {
                            if (const ItemInstance* inst = state.items.get(sel->vid)) {
                                std::string nm = "item";
                                std::string desc;
                                uint32_t maxc = 1;
                                bool consume = false;
                                int sid = -1;
                                if (g_lua_mgr) {
                                    for (auto const& d : g_lua_mgr->items())
                                        if (d.type == inst->def_type) {
                                            nm = d.name;
                                            desc = d.desc;
                                            maxc = (uint32_t)d.max_count;
                                            consume = d.consume_on_use;
                                            if (!d.sprite.empty() && d.sprite.find(':') != std::string::npos && g_sprite_ids)
                                                sid = g_sprite_ids->try_get(d.sprite);
                                            break;
                                        }
                                }
                                if (sid >= 0) if (SDL_Texture* tex = textures.get(sid)) { SDL_Rect dst{tx, ty, 48, 32}; SDL_RenderCopy(renderer, tex, nullptr, &dst); ty += 36; }
                                draw_txt(std::string("Item: ") + nm, SDL_Color{255, 255, 255, 255});
                                draw_txt(std::string("Count: ") + std::to_string(inst->count) + "/" + std::to_string(maxc), SDL_Color{220, 220, 220, 255});
                                if (!desc.empty()) draw_txt(std::string("Desc: ") + desc, SDL_Color{200, 200, 200, 255});
                                draw_txt(std::string("Consumable: ") + (consume ? "Yes" : "No"), SDL_Color{220, 220, 220, 255});
                            }
                        } else if (sel->kind == INV_GUN) {
                            if (const GunInstance* gi = state.guns.get(sel->vid)) {
                                std::string nm = "gun";
                                const GunDef* gdp = nullptr;
                                if (g_lua_mgr) {
                                    for (auto const& g : g_lua_mgr->guns()) if (g.type == gi->def_type) { gdp = &g; break; }
                                }
                                if (gdp) {
                                    int gun_sid = -1;
                                    if (!gdp->sprite.empty() && g_sprite_ids) gun_sid = g_sprite_ids->try_get(gdp->sprite);
                                    if (gun_sid >= 0) if (SDL_Texture* tex = textures.get(gun_sid)) { SDL_Rect dst{tx, ty, 64, 40}; SDL_RenderCopy(renderer, tex, nullptr, &dst); ty += 44; }
                                    draw_txt(std::string("Gun: ") + gdp->name, SDL_Color{255, 255, 255, 255});
                                    draw_txt(std::string("Damage: ") + std::to_string((int)std::lround(gdp->damage)), SDL_Color{220, 220, 220, 255});
                                    draw_txt(std::string("RPM: ") + std::to_string((int)std::lround(gdp->rpm)), SDL_Color{220, 220, 220, 255});
                                    // extra stats
                                    draw_txt(std::string("Deviation: ") + std::to_string(gdp->deviation) + " deg", SDL_Color{220, 220, 220, 255});
                                    draw_txt(std::string("Pellets: ") + std::to_string(gdp->pellets_per_shot), SDL_Color{220, 220, 220, 255});
                                    draw_txt(std::string("Recoil: ") + std::to_string(gdp->recoil), SDL_Color{220, 220, 220, 255});
                                    draw_txt(std::string("Control: ") + std::to_string(gdp->control), SDL_Color{220, 220, 220, 255});
                                    draw_txt(std::string("Recoil cap: ") + std::to_string((int)std::lround(gdp->max_recoil_spread_deg)) + " deg", SDL_Color{220, 220, 220, 255});
                                    draw_txt(std::string("Reload/Eject: ") + std::to_string((int)std::lround(gdp->reload_time * 1000.0f)) + "/" + std::to_string((int)std::lround(gdp->eject_time * 1000.0f)) + " ms", SDL_Color{220, 220, 220, 255});
                                    draw_txt(std::string("Jam: ") + std::to_string((int)std::lround(gdp->jam_chance * 100.0f)) + " %", SDL_Color{220, 220, 220, 255});
                                    // AR window details
                                    draw_txt(std::string("AR Center: ") + std::to_string(gdp->ar_pos) +
                                                 std::string(" ±") + std::to_string(gdp->ar_pos_variance), SDL_Color{220, 220, 220, 255});
                                    draw_txt(std::string("AR Size: ") + std::to_string(gdp->ar_size) +
                                                 std::string(" ±") + std::to_string(gdp->ar_size_variance), SDL_Color{220, 220, 220, 255});
                                    // ammo for hovered gun if it has one (uses its instance)
                                    if (gi->ammo_type != 0) {
                                        if (auto const* ad = g_lua_mgr->find_ammo(gi->ammo_type)) {
                                            int asid = (!ad->sprite.empty() && g_sprite_ids) ? g_sprite_ids->try_get(ad->sprite) : -1;
                                            if (asid >= 0) if (SDL_Texture* tex = textures.get(asid)) { SDL_Rect dst{tx, ty, 36, 20}; SDL_RenderCopy(renderer, tex, nullptr, &dst); ty += 22; }
                                            draw_txt(std::string("Ammo: ") + ad->name, SDL_Color{255, 255, 255, 255});
                                            if (!ad->desc.empty()) draw_txt(std::string("Desc: ") + ad->desc, SDL_Color{200, 200, 200, 255});
                                            int apct = (int)std::lround(ad->armor_pen * 100.0f);
                                            draw_txt(std::string("Ammo Stats: DMG x") + std::to_string(ad->damage_mult) +
                                                         std::string(", AP ") + std::to_string(apct) + "%" +
                                                         std::string(", Shield x") + std::to_string(ad->shield_mult), SDL_Color{220, 220, 220, 255});
                                            if (ad->range_units > 0.0f) {
                                                draw_txt(std::string("Range: ") + std::to_string((int)std::lround(ad->range_units)) +
                                                             std::string(" (falloff ") + std::to_string((int)std::lround(ad->falloff_start)) +
                                                             std::string("→") + std::to_string((int)std::lround(ad->falloff_end)) +
                                                             std::string(" @ x") + std::to_string(ad->falloff_min_mult) + ")", SDL_Color{220, 220, 220, 255});
                                            }
                                            draw_txt(std::string("Speed: ") + std::to_string((int)std::lround(ad->speed)) +
                                                         std::string(", Pierce: ") + std::to_string(ad->pierce_count), SDL_Color{220, 220, 220, 255});
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }

            // Right-side selected item details panel removed in favor of hover center panel

            // Right-side gun panel (player's equipped) – gameplay only
            if (ui_font && state.mode == ids::MODE_PLAYING && state.player_vid && g_lua_mgr && state.show_gun_panel) {
                const Entity* ply = state.entities.get(*state.player_vid);
                if (ply && ply->equipped_gun_vid.has_value()) {
                    // lookup gun def by type
                    const GunDef* gd = nullptr;
                    const GunInstance* gi_inst = state.guns.get(*ply->equipped_gun_vid);
                    if (gi_inst) {
                        for (auto const& g : g_lua_mgr->guns())
                            if (g.type == gi_inst->def_type) {
                                gd = &g;
                                break;
                            }
                    }
                    if (gd) {
                        int panel_w = (int)std::lround(width * 0.26);
                        int px = width - panel_w - 30;
                        int py = (int)std::lround(height * 0.18);
                        SDL_Rect box{px, py, panel_w, 460};
                        SDL_SetRenderDrawColor(renderer, 25, 25, 30, 220);
                        SDL_RenderFillRect(renderer, &box);
                        SDL_SetRenderDrawColor(renderer, 200, 200, 220, 255);
                        SDL_RenderDrawRect(renderer, &box);
                        int tx = px + 12;
                        int ty = py + 12;
                        int lh = 18;
                        auto draw_txt = [&](const std::string& s, SDL_Color col) {
                            SDL_Surface* srf = TTF_RenderUTF8_Blended(ui_font, s.c_str(), col);
                            if (srf) {
                                SDL_Texture* t = SDL_CreateTextureFromSurface(renderer, srf);
                                int tw = 0, th = 0;
                                SDL_QueryTexture(t, nullptr, nullptr, &tw, &th);
                                SDL_Rect d{tx, ty, tw, th};
                                SDL_RenderCopy(renderer, t, nullptr, &d);
                                SDL_DestroyTexture(t);
                                SDL_FreeSurface(srf);
                            }
                            ty += lh;
                        };
                        auto fmt2 = [](float v){ char buf[32]; std::snprintf(buf,sizeof(buf),"%.2f",(double)v); return std::string(buf); };
                        auto draw_kv = [&](const char* key, const std::string& value){
                            SDL_Color kc{150,150,150,255}; SDL_Color vc{230,230,230,255};
                            int save_ty = ty; // key
                            SDL_Surface* ks = TTF_RenderUTF8_Blended(ui_font, (std::string(key)+": ").c_str(), kc);
                            int keyw=0,keyh=0; SDL_Texture* kt=nullptr; if (ks){ kt=SDL_CreateTextureFromSurface(renderer,ks); SDL_QueryTexture(kt,nullptr,nullptr,&keyw,&keyh); SDL_Rect kd{tx, save_ty, keyw, keyh}; SDL_RenderCopy(renderer, kt, nullptr, &kd); SDL_DestroyTexture(kt); SDL_FreeSurface(ks);} 
                            // value
                            SDL_Surface* vs = TTF_RenderUTF8_Blended(ui_font, value.c_str(), vc);
                            if (vs){ SDL_Texture* vt=SDL_CreateTextureFromSurface(renderer,vs); int vw=0,vh=0; SDL_QueryTexture(vt,nullptr,nullptr,&vw,&vh); SDL_Rect vd{tx+keyw, save_ty, vw, vh}; SDL_RenderCopy(renderer, vt, nullptr, &vd); SDL_DestroyTexture(vt); SDL_FreeSurface(vs);} 
                            ty += lh;
                        };
                        // Gun sprite icon
                        if (!gd->sprite.empty() && g_sprite_ids) {
                            int sid = g_sprite_ids->try_get(gd->sprite);
                            if (sid >= 0) {
                                if (SDL_Texture* tex = textures.get(sid)) {
                                    SDL_Rect dst{tx, ty, 64, 40};
                                    SDL_RenderCopy(renderer, tex, nullptr, &dst);
                                    ty += 44;
                                }
                            }
                        }
                        draw_kv("Gun", gd->name);
                        draw_kv("Damage", std::to_string((int)std::lround(gd->damage)));
                        draw_kv("RPM", std::to_string((int)std::lround(gd->rpm)));
                        // Extra gun stats
                        draw_kv("Deviation", fmt2(gd->deviation) + " deg");
                        draw_kv("Pellets", std::to_string(gd->pellets_per_shot));
                        draw_kv("Recoil cap", std::to_string((int)std::lround(gd->max_recoil_spread_deg)) + " deg");
                        draw_kv("Reload", std::to_string((int)std::lround(gd->reload_time * 1000.0f)) + " ms");
                        draw_kv("Eject", std::to_string((int)std::lround(gd->eject_time * 1000.0f)) + " ms");
                        draw_kv("Jam", std::to_string((int)std::lround(gd->jam_chance * 100.0f)) + " %");
                        // AR window details
                        ui_draw_kv_line(renderer, ui_font, tx, ty, lh, "AR Center", fmt2(gd->ar_pos) + " ±" + fmt2(gd->ar_pos_variance));
                        ui_draw_kv_line(renderer, ui_font, tx, ty, lh, "AR Size", fmt2(gd->ar_size) + " ±" + fmt2(gd->ar_size_variance));
                        // Timings
                        if (gd->rpm > 0.0f || gd->shot_interval > 0.0f) {
                            float dt = gd->shot_interval > 0.0f ? gd->shot_interval : (60.0f / std::max(1.0f, gd->rpm));
                            int ms = (int)std::lround(dt * 1000.0f);
                            draw_txt(std::string("Shot Time: ") + std::to_string(ms) + " ms",
                                     SDL_Color{220, 220, 220, 255});
                        }
                        if (gd->fire_mode == std::string("burst") && (gd->burst_rpm > 0.0f || gd->burst_interval > 0.0f)) {
                            draw_txt(std::string("Burst RPM: ") + std::to_string((int)std::lround(gd->burst_rpm)),
                                     SDL_Color{220, 220, 220, 255});
                            float bdt = gd->burst_interval > 0.0f ? gd->burst_interval : (gd->burst_rpm > 0.0f ? 60.0f / gd->burst_rpm : 0.0f);
                            if (bdt > 0.0f) {
                                int bms = (int)std::lround(bdt * 1000.0f);
                                draw_txt(std::string("Burst Time: ") + std::to_string(bms) + " ms",
                                         SDL_Color{220, 220, 220, 255});
                            }
                        }
                        // Fire mode label
                        {
                            std::string fm_label = "Auto";
                            if (gd->fire_mode == "single")
                                fm_label = "Semi";
                            else if (gd->fire_mode == "burst") {
                                fm_label = "Burst";
                                if (gd->burst_count > 0)
                                    fm_label += std::string(" (") + std::to_string(gd->burst_count) + ")";
                            }
                            draw_txt(std::string("Fire: ") + fm_label, SDL_Color{220, 220, 220, 255});
                        }
                        draw_txt(std::string("Recoil: ") + fmt2(gd->recoil), SDL_Color{220,220,220,255});
                        draw_txt(std::string("Control: ") + fmt2(gd->control), SDL_Color{220,220,220,255});
                        // Show instance ammo
                        if (gi_inst) {
                            ui_draw_kv_line(renderer, ui_font, tx, ty, lh, "Mag", std::to_string(gi_inst->current_mag));
                            ui_draw_kv_line(renderer, ui_font, tx, ty, lh, "Reserve", std::to_string(gi_inst->ammo_reserve));
                            if (gi_inst->ammo_type != 0) {
                                if (auto const* ad = g_lua_mgr->find_ammo(gi_inst->ammo_type)) {
                                    // ammo sprite
                                    if (!ad->sprite.empty() && g_sprite_ids) {
                                        int asid = g_sprite_ids->try_get(ad->sprite);
                                        if (asid >= 0) {
                                            if (SDL_Texture* tex = textures.get(asid)) {
                                                SDL_Rect dst{tx, ty, 36, 20};
                                                SDL_RenderCopy(renderer, tex, nullptr, &dst);
                                                ty += 22;
                                            }
                                        }
                                    }
                                    ui_draw_kv_line(renderer, ui_font, tx, ty, lh, "Ammo", ad->name);
                                    if (!ad->desc.empty()) ui_draw_kv_line(renderer, ui_font, tx, ty, lh, "Desc", ad->desc);
                                    int apct = (int)std::lround(ad->armor_pen * 100.0f);
                                    // indent ammo stats
                                    int save_tx = tx; tx += 10;
                                    ui_draw_kv_line(renderer, ui_font, tx, ty, lh, "DMG", fmt2(ad->damage_mult));
                                    ui_draw_kv_line(renderer, ui_font, tx, ty, lh, "AP", std::to_string(apct) + "%");
                                    ui_draw_kv_line(renderer, ui_font, tx, ty, lh, "Shield", fmt2(ad->shield_mult));
                                    if (ad->range_units > 0.0f) {
                                        ui_draw_kv_line(renderer, ui_font, tx, ty, lh, "Range", std::to_string((int)std::lround(ad->range_units)));
                                        ui_draw_kv_line(renderer, ui_font, tx, ty, lh, "Falloff", std::to_string((int)std::lround(ad->falloff_start)) + "→" + std::to_string((int)std::lround(ad->falloff_end)));
                                        ui_draw_kv_line(renderer, ui_font, tx, ty, lh, "Min Mult", fmt2(ad->falloff_min_mult));
                                    }
                                    ui_draw_kv_line(renderer, ui_font, tx, ty, lh, "Speed", std::to_string((int)std::lround(ad->speed)));
                                    ui_draw_kv_line(renderer, ui_font, tx, ty, lh, "Pierce", std::to_string(ad->pierce_count));
                                    tx = save_tx;
                                }
                            }
                        }
                    }
                }
            }

            // Bottom player condition bars (shield, plates, health) with numbers and fixed width
            if (ui_font && state.mode == ids::MODE_PLAYING && state.player_vid) {
                const Entity* p = state.entities.get(*state.player_vid);
                if (p) {
                    int group_w = std::max(200, (int)std::lround(width * 0.25));
                    int bar_h = 16;
                    int gap_y = 6;
                    int total_h = bar_h * 3 + gap_y * 2;
                    int gx = (width - group_w) / 2;
                    int gy = height - total_h - 28;
                    if (state.hp_bar_shake > 0.01f) {
                        static thread_local std::mt19937 rng{std::random_device{}()};
                        std::uniform_real_distribution<float> J(-state.hp_bar_shake,
                                                                state.hp_bar_shake);
                        gx += (int)std::lround(J(rng));
                        gy += (int)std::lround(J(rng));
                        state.hp_bar_shake *= 0.90f;
                    } else {
                        state.hp_bar_shake = 0.0f;
                    }
                    auto draw_num = [&](const std::string& s, int x, int y, SDL_Color col) {
                        SDL_Surface* srf = TTF_RenderUTF8_Blended(ui_font, s.c_str(), col);
                        if (!srf)
                            return SDL_Point{x, y};
                        SDL_Texture* t = SDL_CreateTextureFromSurface(renderer, srf);
                        int tw = 0, th = 0;
                        SDL_QueryTexture(t, nullptr, nullptr, &tw, &th);
                        SDL_Rect d{x, y + (bar_h - th) / 2, tw, th};
                        SDL_RenderCopy(renderer, t, nullptr, &d);
                        SDL_DestroyTexture(t);
                        SDL_FreeSurface(srf);
                        return SDL_Point{d.x + d.w, d.y + d.h};
                    };
                    auto draw_bar = [&](int x, int y, int w, int h, float ratio, SDL_Color fill) {
                        SDL_Rect bg{x, y, w, h};
                        SDL_SetRenderDrawColor(renderer, 20, 20, 24, 220);
                        SDL_RenderFillRect(renderer, &bg);
                        SDL_SetRenderDrawColor(renderer, 60, 60, 70, 255);
                        SDL_RenderDrawRect(renderer, &bg);
                        double r = (double)std::clamp(ratio, 0.0f, 1.0f);
                        int fw = (int)std::lround((double)w * r);
                        if (fw > 0) {
                            SDL_Rect fr{x, y, fw, h};
                            SDL_SetRenderDrawColor(renderer, fill.r, fill.g, fill.b, fill.a);
                            SDL_RenderFillRect(renderer, &fr);
                        }
                    };
                    SDL_Color white{240, 240, 240, 255};
                    // Shield (top)
                    if (p->stats.shield_max > 0.0f) {
                        float sratio = (p->stats.shield_max > 0.0f)
                                           ? (p->shield / p->stats.shield_max)
                                           : 0.0f;
                        draw_bar(gx, gy, group_w, bar_h, sratio,
                                 SDL_Color{120, 200, 240, 220});
                        // Numbers: left current, right max
                        draw_num(std::to_string((int)std::lround(p->shield)), gx - 46, gy, white);
                        draw_num(std::to_string((int)std::lround(p->stats.shield_max)),
                                 gx + group_w + 6, gy, white);
                    }
                    // Plates (middle)
                    int gy2 = gy + bar_h + gap_y;
                    {
                        // Background only; fill with slivers up to 20
                        draw_bar(gx, gy2, group_w, bar_h, 0.0f, SDL_Color{0, 0, 0, 0});
                        int to_show = std::min(20, p->stats.plates);
                        int slw = 6;
                        int gap = 2;
                        // Left-align plate slivers (match dash behavior)
                        int start_x = gx;
                        for (int i = 0; i < to_show; ++i) {
                            SDL_Rect prr{start_x + i * (slw + gap), gy2 + 2, slw, bar_h - 4};
                            SDL_SetRenderDrawColor(renderer, 80, 80, 80, 255);
                            SDL_RenderFillRect(renderer, &prr);
                            SDL_SetRenderDrawColor(renderer, 140, 140, 140, 255);
                            SDL_RenderDrawRect(renderer, &prr);
                        }
                        // Current plates count on left
                        draw_num(std::to_string(p->stats.plates), gx - 46, gy2, white);
                    }
                    // Health (bottom)
                    int gy3 = gy2 + bar_h + gap_y;
                    {
                        float hratio = (p->max_hp > 0) ? ((float)p->health / (float)p->max_hp)
                                                       : 0.0f;
                        draw_bar(gx, gy3, group_w, bar_h, hratio, SDL_Color{220, 60, 60, 230});
                        draw_num(std::to_string((int)p->health), gx - 46, gy3, white);
                        draw_num(std::to_string((int)p->max_hp), gx + group_w + 6, gy3, white);
                    }
                    // Dash (bottom-most bar) with small sliver sections and refill sliver above
                    int gy4 = gy3 + bar_h + gap_y;
                    // background bar
                    draw_bar(gx, gy4, group_w, bar_h, 0.0f, SDL_Color{0, 0, 0, 0});
                    if (state.dash_max > 0) {
                        int segs = state.dash_max;
                        int slw = 12; // wider dash slivers
                        int sgap = 2;
                        int start_x = gx; // left-aligned slivers
                        for (int i = 0; i < segs; ++i) {
                            int sx = start_x + i * (slw + sgap);
                            SDL_Rect seg{sx, gy4 + 2, slw, bar_h - 4};
                            if (i < state.dash_stocks)
                                SDL_SetRenderDrawColor(renderer, 80, 200, 120, 220);
                            else
                                SDL_SetRenderDrawColor(renderer, 40, 60, 70, 200);
                            SDL_RenderFillRect(renderer, &seg);
                            SDL_SetRenderDrawColor(renderer, 20, 30, 40, 255);
                            SDL_RenderDrawRect(renderer, &seg);
                        }
                        // Refill progress sliver (only when below max)
                        if (state.dash_stocks < state.dash_max) {
                            double pratio = std::clamp((double)(state.dash_refill_timer / DASH_COOLDOWN_SECONDS), 0.0, 1.0);
                            int pw = (int)std::lround((double)group_w * pratio);
                            int psl = std::max(2, bar_h / 4);
                            // Place the refill bar immediately above the dash bar (flush against it)
                            SDL_Rect pbar{gx, gy4 - psl, pw, psl};
                            SDL_SetRenderDrawColor(renderer, 90, 200, 160, 200);
                            SDL_RenderFillRect(renderer, &pbar);
                        }
                    }
                }
            }

            // Alerts (top-most): persistent alerts then one-frame warnings
            if (ui_font) {
                int ax = 20;
                int ay = 90;
                int lh = 18;
                for (const auto& al : state.alerts) {
                    SDL_Color c{200, 200, 220, 255};
                    SDL_Surface* s = TTF_RenderUTF8_Blended(ui_font, al.text.c_str(), c);
                    if (s) {
                        SDL_Texture* t = SDL_CreateTextureFromSurface(renderer, s);
                        int tw = 0, th = 0;
                        SDL_QueryTexture(t, nullptr, nullptr, &tw, &th);
                        SDL_Rect d{ax, ay, tw, th};
                        SDL_RenderCopy(renderer, t, nullptr, &d);
                        SDL_DestroyTexture(t);
                        SDL_FreeSurface(s);
                    }
                    ay += lh;
                }
                for (const auto& msg : frame_warnings) {
                    SDL_Color c{220, 60, 60, 255};
                    SDL_Surface* s = TTF_RenderUTF8_Blended(ui_font, msg.c_str(), c);
                    if (s) {
                        SDL_Texture* t = SDL_CreateTextureFromSurface(renderer, s);
                        int tw = 0, th = 0;
                        SDL_QueryTexture(t, nullptr, nullptr, &tw, &th);
                        SDL_Rect d{ax, ay, tw, th};
                        SDL_RenderCopy(renderer, t, nullptr, &d);
                        SDL_DestroyTexture(t);
                        SDL_FreeSurface(s);
                    }
                    ay += lh;
                }
            }

            SDL_RenderPresent(renderer);
        } else {
            SDL_Delay(16);
        }
        #endif

        // FPS calculation using high-resolution timer
        // dt_sec computed above
        accum_sec += dt_sec;
        frame_counter += 1;
        if (accum_sec >= 1.0) {
            last_fps = frame_counter;
            frame_counter = 0;
            accum_sec -= 1.0;
            title_buf.clear();
            title_buf.reserve(64);
            title_buf = "gub - FPS: ";
            // Convert FPS to string without iostreams to keep it light
            char tmp[32];
            std::snprintf(tmp, sizeof(tmp), "%d", last_fps);
            title_buf += tmp;
            SDL_SetWindowTitle(window, title_buf.c_str());
        }

        // Auto-exit after a fixed number of frames if requested.
        if (arg_frames >= 0) {
            if (--arg_frames <= 0)
                running = false;
        }
    }

    if (renderer) {
        SDL_DestroyRenderer(renderer);
    }
    SDL_DestroyWindow(window);
    if (ui_font)
        TTF_CloseFont(ui_font);
    sounds.shutdown();
    if (TTF_WasInit())
        TTF_Quit();
    SDL_Quit();
    return 0;
}
// generate_room and spawn-safety helpers moved to room.cpp
