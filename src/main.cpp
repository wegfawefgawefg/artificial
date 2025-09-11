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
#include "room.hpp"

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

    const char* title = "gub";
    int width = 1280;
    int height = 720;

    // Initialize Graphics and expose it globally (handles SDL video init too)
    Graphics gfx{};
    g_gfx = &gfx;
    if (!init_graphics(gfx, arg_headless, title, width, height)) {
        SDL_Quit();
        return 1;
    }
    SDL_Window* window = gfx.window;
    SDL_Renderer* renderer = gfx.renderer;

    const char* active_driver = SDL_GetCurrentVideoDriver();
    std::printf("SDL video driver: %s\n", active_driver ? active_driver : "(none)");

    // quick GLM sanity check
    glm::vec3 a(1.0f, 2.0f, 3.0f);
    glm::vec3 b(4.0f, 5.0f, 6.0f);
    float dot = glm::dot(a, b);
    std::printf("glm dot(a,b) = %f\n", static_cast<double>(dot));

    // Engine state/graphics
    State state{};
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
    if (!arg_headless && renderer)
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
                    if (renderer) SDL_GetRendererOutputSize(renderer, &ww, &wh);
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
                if (renderer) SDL_GetRendererOutputSize(renderer, &ww, &wh);
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
        // Render a full frame via renderer module (windowed mode only)
        if (!arg_headless)
            render_frame(window, renderer, textures, ui_font, state, gfx, dt_sec, binds, projectiles, sounds);


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
            if (!arg_headless && window)
                SDL_SetWindowTitle(window, title_buf.c_str());
        }

        // Auto-exit after a fixed number of frames if requested.
        if (arg_frames >= 0) {
            if (--arg_frames <= 0)
                running = false;
        }
    }

    shutdown_graphics(gfx);
    if (ui_font)
        TTF_CloseFont(ui_font);
    sounds.shutdown();
    if (TTF_WasInit())
        TTF_Quit();
    SDL_Quit();
    return 0;
}
