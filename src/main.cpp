// Minimal SDL2 window + GLM usage
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <random>
#include <cmath>
#include <algorithm>
#include <filesystem>

#include <glm/glm.hpp>

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

#include "state.hpp"
#include "graphics.hpp"
#include "input_system.hpp"
#include "projectiles.hpp"
#include "config.hpp"
#include "mods.hpp"
#include "sprites.hpp"
#include "settings.hpp"
#include "luamgr.hpp"
#include "tex.hpp"

// Helpers
struct Projectiles; // fwd
static void generate_room(State& state, Projectiles& projectiles, SDL_Renderer* renderer, Graphics& gfx);
static LuaManager* g_lua_mgr = nullptr;
static SpriteIdRegistry* g_sprite_ids = nullptr;
static bool tile_blocks_entity(const State& state, int x, int y);
static glm::ivec2 nearest_walkable_tile(const State& state, glm::ivec2 t, int max_radius);
static glm::vec2 ensure_not_in_block(const State& state, glm::vec2 pos);

static bool try_init_video_with_driver(const char *driver)
{
  if (driver)
  {
    setenv("SDL_VIDEODRIVER", driver, 1);
  }
  if (SDL_Init(SDL_INIT_VIDEO) == 0)
    return true;
  const char *err = SDL_GetError();
  std::fprintf(stderr, "SDL_Init failed (driver=%s): %s\n",
               driver ? driver : "auto",
               (err && *err) ? err : "(no error text)");
  return false;
}

int main(int argc, char** argv)
{
  // Lightweight CLI args for non-interactive testing
  bool arg_scan_sprites = false;
  bool arg_headless = false;
  long arg_frames = -1; // <0 => unlimited
  for (int i=1;i<argc;++i) {
    std::string a(argv[i]);
    if (a == "--scan-sprites") arg_scan_sprites = true;
    else if (a == "--headless") arg_headless = true;
    else if (a.rfind("--frames=", 0) == 0) {
      std::string v = a.substr(9);
      try { arg_frames = std::stol(v); } catch (...) { arg_frames = -1; }
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
      if (shown++ >= 10) break;
      std::printf(" - %s\n", n.c_str());
    }
    return 0;
  }

  const char *env_display = std::getenv("DISPLAY");
  const char *env_wayland = std::getenv("WAYLAND_DISPLAY");
  const char *env_sdl_driver = std::getenv("SDL_VIDEODRIVER");

  if (arg_headless) env_sdl_driver = "dummy";

  if (!try_init_video_with_driver(env_sdl_driver))
  {
    if (env_display && *env_display)
    {
      if (try_init_video_with_driver("x11"))
        goto init_ok;
    }
    if (env_wayland && *env_wayland)
    {
      if (try_init_video_with_driver("wayland"))
        goto init_ok;
    }
    // Last resort: dummy driver for headless environments
    if (try_init_video_with_driver("dummy"))
    {
      std::fprintf(stderr, "Using SDL dummy video driver; no window will be shown.\n");
    }
    else
    {
      return 1;
    }
  }
init_ok:

  const char *title = "gub";
  int width = 1280;
  int height = 720;

  const char *active_driver = SDL_GetCurrentVideoDriver();
  std::printf("SDL video driver: %s\n", active_driver ? active_driver : "(none)");

  Uint32 win_flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALWAYS_ON_TOP | SDL_WINDOW_UTILITY;
  SDL_Window *window = SDL_CreateWindow(title,
                                        SDL_WINDOWPOS_CENTERED,
                                        SDL_WINDOWPOS_CENTERED,
                                        width,
                                        height,
                                        win_flags);
  if (!window)
  {
    const char *err = SDL_GetError();
    std::fprintf(stderr, "SDL_CreateWindow failed: %s\n", (err && *err) ? err : "(no error text)");
    SDL_Quit();
    return 1;
  }

  // Ensure always-on-top in case WM didn't honor the flag immediately
  SDL_SetWindowAlwaysOnTop(window, SDL_TRUE);

  // Create a renderer so we can paint a background instead of showing desktop
  SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
  if (!renderer)
  {
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
        if (ec) { ec.clear(); continue; }
        if (!de.is_regular_file()) continue;
        auto p = de.path();
        auto ext = p.extension().string();
        for (auto& c : ext) c = (char)std::tolower((unsigned char)c);
        if (ext == ".ttf") { font_path = p.string(); break; }
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
  while (running && state.running)
  {
    SDL_Event ev;
    bool request_quit = false;
    ictx.wheel_delta = 0.0f;
    while (SDL_PollEvent(&ev))
    {
      if (ev.type == SDL_WINDOWEVENT && ev.window.event == SDL_WINDOWEVENT_CLOSE) request_quit = true;
      process_events(ev, ictx, state, request_quit);
      if (ev.type == SDL_QUIT) request_quit = true;
    }

    Uint64 t_now = SDL_GetPerformanceCounter();
    double dt_sec = static_cast<double>(t_now - t_last) / static_cast<double>(perf_freq);
    t_last = t_now;

    build_inputs(binds, ictx, state, gfx, static_cast<float>(dt_sec));
    // Age alerts and purge expired
    for (auto &al : state.alerts) { al.age += static_cast<float>(dt_sec); }
    state.alerts.erase(std::remove_if(state.alerts.begin(), state.alerts.end(), [](const State::Alert& al){ return al.purge_eof || (al.ttl >= 0.0f && al.age > al.ttl); }), state.alerts.end());
    // Hot reload poll (assets + behaviors stubs)
    mods.poll_hot_reload(sprites, dt_sec);
    mods.poll_hot_reload(sprite_store, dt_sec);

    if (request_quit) running = false;

    // Update: fixed timestep simulation
    state.time_since_last_update += static_cast<float>(dt_sec);
    while (state.time_since_last_update > TIMESTEP) {
      state.time_since_last_update -= TIMESTEP;

      // Movement + physics: player controlled + NPC wander; keep inside non-block tiles
      for (auto& e : state.entities.data()) {
        if (!e.active) continue;
        if (e.type_ == ids::ET_PLAYER) {
          glm::vec2 dir{0.0f, 0.0f};
          if (state.playing_inputs.left) dir.x -= 1.0f;
          if (state.playing_inputs.right) dir.x += 1.0f;
          if (state.playing_inputs.up) dir.y -= 1.0f;
          if (state.playing_inputs.down) dir.y += 1.0f;
          if (dir.x != 0.0f || dir.y != 0.0f) dir = glm::normalize(dir);
          // Scale base speed by stats.move_speed (350 baseline)
          float scale = (e.stats.move_speed > 0.0f) ? (e.stats.move_speed / 350.0f) : 1.0f;
          e.vel = dir * (PLAYER_SPEED_UNITS_PER_SEC * scale);
        } else {
          // NPC random drift
          if (e.rot <= 0.0f) {
            static thread_local std::mt19937 rng{std::random_device{}()};
            std::uniform_int_distribution<int> dirD(0,4);
            std::uniform_real_distribution<float> dur(0.5f, 2.0f);
            int dir = dirD(rng);
            glm::vec2 v{0,0}; if (dir==0) v={-1,0}; else if(dir==1) v={1,0}; else if(dir==2) v={0,-1}; else if(dir==3) v={0,1};
            e.vel = v * 2.0f; // 2 units/s
            e.rot = dur(rng); // reuse rot as timer here for simplicity
          } else {
            e.rot -= TIMESTEP;
          }
        }
        int steps = std::max(1, e.physics_steps);
        glm::vec2 step_dpos = e.vel * ( TIMESTEP / static_cast<float>(steps));
        for (int s=0;s<steps;++s) {
          // X axis
          float next_x = e.pos.x + step_dpos.x;
          {
            glm::vec2 half = e.half_size();
            glm::vec2 tl = {next_x - half.x, e.pos.y - half.y};
            glm::vec2 br = {next_x + half.x, e.pos.y + half.y};
            int minx = (int)floorf(tl.x), miny=(int)floorf(tl.y), maxx=(int)floorf(br.x), maxy=(int)floorf(br.y);
            bool blocked=false;
            for (int y=miny;y<=maxy && !blocked;++y) for (int x=minx;x<=maxx;++x)
              if (state.stage.in_bounds(x,y) && state.stage.at(x,y).blocks_entities()) { blocked=true; }
            if (!blocked) e.pos.x = next_x; else { e.vel.x = 0.0f; }
          }
          // Y axis
          float next_y = e.pos.y + step_dpos.y;
          {
            glm::vec2 half = e.half_size();
            glm::vec2 tl = {e.pos.x - half.x, next_y - half.y};
            glm::vec2 br = {e.pos.x + half.x, next_y + half.y};
            int minx = (int)floorf(tl.x), miny=(int)floorf(tl.y), maxx=(int)floorf(br.x), maxy=(int)floorf(br.y);
            bool blocked=false;
            for (int y=miny;y<=maxy && !blocked;++y) for (int x=minx;x<=maxx;++x)
              if (state.stage.in_bounds(x,y) && state.stage.at(x,y).blocks_entities()) { blocked=true; }
            if (!blocked) e.pos.y = next_y; else { e.vel.y = 0.0f; }
          }
        }
      }

      // Auto-pickup powerups on overlap
      if (state.player_vid) {
        const Entity* p = state.entities.get(*state.player_vid);
        if (p) {
          glm::vec2 ph = p->half_size();
          float pl = p->pos.x - ph.x, pr = p->pos.x + ph.x;
          float pt = p->pos.y - ph.y, pb = p->pos.y + ph.y;
          for (auto& pu : state.pickups.data()) if (pu.active) {
            float gl = pu.pos.x - 0.125f, gr = pu.pos.x + 0.125f;
            float gt = pu.pos.y - 0.125f, gb = pu.pos.y + 0.125f;
            bool overlap = !(pr <= gl || pl >= gr || pb <= gt || pt >= gb);
            if (overlap) {
              state.alerts.push_back({ std::string("Picked up ") + pu.name, 0.0f, 2.0f, false });
              pu.active = false;
            }
          }
        }
      }

      // Item pickup with F key; swap if inventory full; also ground guns equip
      if (state.player_vid) {
        const Entity* p = state.entities.get(*state.player_vid);
        if (p) {
          glm::vec2 ph = p->half_size();
          float pl = p->pos.x - ph.x, pr = p->pos.x + ph.x;
          float pt = p->pos.y - ph.y, pb = p->pos.y + ph.y;
          if (state.playing_inputs.pick_up) {
            // Guns first: equip directly
            if (auto* pm = state.entities.get_mut(*state.player_vid)) {
              for (auto& gg : state.ground_guns.data()) if (gg.active) {
                glm::vec2 gh = gg.size*0.5f; float gl=gg.pos.x-gh.x, gr=gg.pos.x+gh.x; float gt=gg.pos.y-gh.y, gb=gg.pos.y+gh.y;
                bool overlap = !(pr <= gl || pl >= gr || pb <= gt || pt >= gb);
                if (overlap && gg.gun_inst_id >= 0) {
                  pm->primary_gun_inst = gg.gun_inst_id; gg.active=false;
                  state.alerts.push_back({"Equipped ground gun", 0.0f, 2.0f, false});
                  break;
                }
              }
            }
            for (auto& gi : state.ground_items.data()) if (gi.active) {
              glm::vec2 gh = gi.size * 0.5f;
              float gl = gi.pos.x - gh.x, gr = gi.pos.x + gh.x;
              float gt = gi.pos.y - gh.y, gb = gi.pos.y + gh.y;
              bool overlap = !(pr <= gl || pl >= gr || pb <= gt || pt >= gb);
              if (overlap) {
                auto leftover = state.inventory.insert(gi.item);
                if (leftover.has_value()) {
                  // Could not fully add; swap or drop leftover
                  gi.item = *leftover;
                  state.alerts.push_back({ std::string("Swapped: ") + gi.item.name, 0.0f, 2.0f, false });
                } else {
                  state.alerts.push_back({ std::string("Picked up ") + gi.item.name, 0.0f, 2.0f, false });
                  gi.active = false;
                }
                break;
              }
            }
          }
          // Simple separation to avoid intersecting ground items
          for (auto& giA : state.ground_items.data()) if (giA.active) {
            for (auto& giB : state.ground_items.data()) if (&giA != &giB && giB.active) {
              glm::vec2 ah = giA.size * 0.5f, bh = giB.size * 0.5f;
              bool overlap = !( (giA.pos.x+ah.x) <= (giB.pos.x-bh.x) || (giA.pos.x-ah.x) >= (giB.pos.x+bh.x)
                             || (giA.pos.y+ah.y) <= (giB.pos.y-bh.y) || (giA.pos.y-ah.y) >= (giB.pos.y+bh.y) );
              if (overlap) {
                glm::vec2 d = giA.pos - giB.pos; if (d.x==0 && d.y==0) d = {0.01f,0.0f};
                float len = std::sqrt(d.x*d.x + d.y*d.y); if (len < 1e-3f) len = 1.0f; d /= len;
                giA.pos += d * 0.01f; giB.pos -= d * 0.01f;
              }
            }
          }
          for (auto& ga : state.ground_guns.data()) if (ga.active) {
            for (auto& gb : state.ground_guns.data()) if (&ga != &gb && gb.active) {
              glm::vec2 ah = ga.size * 0.5f, bh = gb.size * 0.5f;
              bool overlap = !( (ga.pos.x+ah.x) <= (gb.pos.x-bh.x) || (ga.pos.x-ah.x) >= (gb.pos.x+bh.x)
                             || (ga.pos.y+ah.y) <= (gb.pos.y-bh.y) || (ga.pos.y-ah.y) >= (gb.pos.y+bh.y) );
              if (overlap) {
                glm::vec2 d = ga.pos - gb.pos; if (d.x==0 && d.y==0) d = {0.01f,0.0f};
                float len = std::sqrt(d.x*d.x + d.y*d.y); if (len < 1e-3f) len = 1.0f; d /= len;
                ga.pos += d * 0.01f; gb.pos -= d * 0.01f;
              }
            }
          }
        }
      }

      // Number keys: select slot then act based on item category
      {
        bool nums[10] = {
          state.playing_inputs.num_row_1,
          state.playing_inputs.num_row_2,
          state.playing_inputs.num_row_3,
          state.playing_inputs.num_row_4,
          state.playing_inputs.num_row_5,
          state.playing_inputs.num_row_6,
          state.playing_inputs.num_row_7,
          state.playing_inputs.num_row_8,
          state.playing_inputs.num_row_9,
          state.playing_inputs.num_row_0
        };
        static bool prev[10] = {false};
        for (int i=0;i<10;++i) {
          if (nums[i] && !prev[i]) {
            // Map: key1..key9 -> slots 0..8, key0 -> slot 9
            std::size_t idx = (i==9) ? 9 : (std::size_t)i;
            state.inventory.set_selected_index(idx);
            const InvEntry* ent = state.inventory.selected_entry();
            if (ent) {
              // Category: 2=gun -> equip; 1=usable -> use
              if (ent->item.category == 2) {
                // Equip gun
                if (state.player_vid) if (auto* pl = state.entities.get_mut(*state.player_vid)) {
                  pl->primary_gun_id = (int)ent->item.type;
                  state.alerts.push_back({ std::string("Equipped gun: ") + ent->item.name, 0.0f, 2.0f, false });
                }
              } else if (ent->item.category == 1) {
                // Use consumable via Lua on_use if present
                if (g_lua_mgr && state.player_vid) {
                  if (auto* pl = state.entities.get_mut(*state.player_vid)) {
                    (void)g_lua_mgr->call_item_on_use((int)ent->item.type, state, *pl, nullptr);
                  }
                }
                state.alerts.push_back({ std::string("Used ") + ent->item.name, 0.0f, 2.0f, false });
                if (ent->item.consume_on_use) state.inventory.remove_count_from_slot(idx, 1);
              }
            }
          }
          prev[i] = nums[i];
        }
      }

      // Exit countdown and mode transitions
      if (state.mode == ids::MODE_PLAYING) {
        const Entity* p = (state.player_vid ? state.entities.get(*state.player_vid) : nullptr);
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
            state.alerts.push_back({"Exit reached: hold to leave", 0.0f, 2.0f, false});
            std::printf("[room] Exit reached, starting %.1fs countdown...\n", (double)EXIT_COUNTDOWN_SECONDS);
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
          }
        }
      } else if (state.mode == ids::MODE_SCORE_REVIEW) {
        if (state.score_ready_timer > 0.0f) state.score_ready_timer -= TIMESTEP;
      }

      // Camera follow: move towards mix of player and mouse world position
      if (state.player_vid) {
        const Entity* p = state.entities.get(*state.player_vid);
        if (p) {
          int ww=width, wh=height; SDL_GetRendererOutputSize(renderer, &ww, &wh);
          float zx = gfx.play_cam.zoom;
          float sx = static_cast<float>(state.mouse_inputs.pos.x);
          float sy = static_cast<float>(state.mouse_inputs.pos.y);
          // screen pixels to world units
          glm::vec2 mouse_world = p->pos; // default fallback
          float inv_scale = 1.0f / (TILE_SIZE * zx);
          mouse_world.x = gfx.play_cam.pos.x + (sx - static_cast<float>(ww)*0.5f) * inv_scale;
          mouse_world.y = gfx.play_cam.pos.y + (sy - static_cast<float>(wh)*0.5f) * inv_scale;

          glm::vec2 target = p->pos;
          if (state.camera_follow_enabled) {
            target = p->pos + (mouse_world - p->pos) * CAMERA_FOLLOW_FACTOR;
          }
          gfx.play_cam.pos = target;
        }
      }

      // spawn projectile on left mouse with small cooldown (uses gun rpm if equipped)
      state.gun_cooldown = std::max(0.0f, state.gun_cooldown - TIMESTEP);
      bool can_fire = (state.gun_cooldown == 0.0f);
      if (state.mouse_inputs.left && can_fire) {
        // spawn from screen center towards mouse in world-space
        glm::vec2 p = state.player_vid ? state.entities.get(*state.player_vid)->pos
                                       : glm::vec2{ (float)state.stage.get_width()/2.0f, (float)state.stage.get_height()/2.0f };
        // convert mouse to world
        int ww=width, wh=height; SDL_GetRendererOutputSize(renderer, &ww, &wh);
        float inv_scale = 1.0f / (TILE_SIZE * gfx.play_cam.zoom);
        glm::vec2 m = { gfx.play_cam.pos.x + (static_cast<float>(state.mouse_inputs.pos.x) - static_cast<float>(ww)*0.5f) * inv_scale,
                        gfx.play_cam.pos.y + (static_cast<float>(state.mouse_inputs.pos.y) - static_cast<float>(wh)*0.5f) * inv_scale };
        glm::vec2 dir = glm::normalize(m - p);
        if (glm::any(glm::isnan(dir))) dir = {1.0f, 0.0f};
        // If gun equipped, respect ammo and rpm
        float rpm = 600.0f; // default
        bool fired = true;
        if (state.player_vid) {
          auto* plm = state.entities.get_mut(*state.player_vid);
          if (plm && plm->primary_gun_inst >= 0) {
            const GunInstance* gi = state.guns.get(plm->primary_gun_inst);
            const GunDef* gd = nullptr; if (g_lua_mgr && gi) { for (auto const& g : g_lua_mgr->guns()) if (g.type == gi->def_type) { gd=&g; break; } }
            if (gd && gi) {
              rpm = (gd->rpm > 0.0f) ? gd->rpm : rpm;
              GunInstance* gim = state.guns.get_mut(plm->primary_gun_inst);
              if (gim->current_mag <= 0) {
                // naive reload
                int need = gd->mag - gim->current_mag;
                int take = std::min(need, gim->ammo_reserve);
                gim->current_mag += take; gim->ammo_reserve -= take;
              }
              if (gim->current_mag > 0) {
                gim->current_mag -= 1;
              } else {
                fired = false;
              }
            }
          }
        }
        if (fired) {
          auto* pr = projectiles.spawn(p, dir * 20.0f, {0.2f, 0.2f}, 2);
          (void)pr;
          // on_shoot triggers for items in inventory
          if (g_lua_mgr && state.player_vid) {
            auto* plm = state.entities.get_mut(*state.player_vid);
            if (plm) for (const auto& entry : state.inventory.entries) g_lua_mgr->call_item_on_shoot((int)entry.item.type, state, *plm);
          }
          state.gun_cooldown = std::max(0.05f, 60.0f / rpm);
        }
      }

      // step projectiles with on-hit applying damage and drops
      std::vector<std::size_t> hits;
      projectiles.step(TIMESTEP, state.stage, state.entities.data(), [&](Projectile&, const Entity& hit){ hits.push_back(hit.vid.id); });
      for (auto id : hits) {
        if (id >= state.entities.data().size()) continue;
        auto& e = state.entities.data()[id];
        if (!e.active) continue;
        if (e.type_ == ids::ET_NPC) {
          if (e.health == 0) e.health = 3; // initialize default
          if (e.max_hp == 0) e.max_hp = 3;
          // Apply damage with plates and armor
          int dmg = 1; int ap = 0;
          if (e.stats.plates > 0) { e.stats.plates -= 1; dmg = 0; }
          if (dmg > 0) {
            float reduction = std::max(0.0f, e.stats.armor - (float)ap);
            reduction = std::min(75.0f, reduction); // cap benefit
            float scale = 1.0f - reduction * 0.01f;
            int delt = (int)std::ceil((double)dmg * (double)scale);
            e.health = (e.health > (uint32_t)delt) ? (e.health - (uint32_t)delt) : 0u;
          }
          if (e.health == 0) {
            // drop chance
            glm::vec2 pos = e.pos;
            e.active = false;
            // basic drop: 50% chance to drop something
            static thread_local std::mt19937 rng{std::random_device{}()};
            std::uniform_real_distribution<float> U(0.0f, 1.0f);
            if (U(rng) < 0.5f && g_lua_mgr) {
              glm::vec2 place_pos = ensure_not_in_block(state, pos);
              const auto& dt = g_lua_mgr->drops();
              auto pick_weighted = [&](const std::vector<DropEntry>& v)->int{
                if (v.empty()) return -1;
                float sum = 0.0f;
                for (auto const& de : v) sum += de.weight;
                if (sum <= 0.0f) return -1;
                std::uniform_real_distribution<float> du(0.0f, sum);
                float r = du(rng);
                float acc = 0.0f;
                for (auto const& de : v) {
                  acc += de.weight;
                  if (r <= acc) return de.type;
                }
                return v.back().type;
              };
              if (!dt.powerups.empty() || !dt.items.empty() || !dt.guns.empty()) {
                float c = U(rng);
                if (c < 0.5f && !dt.powerups.empty()) {
                  int t = pick_weighted(dt.powerups); if (t>=0) {
                    auto it = std::find_if(g_lua_mgr->powerups().begin(), g_lua_mgr->powerups().end(), [&](const PowerupDef& p){ return p.type==t; });
                    if (it!=g_lua_mgr->powerups().end()) { auto* p = state.pickups.spawn((std::uint32_t)it->type, it->name, place_pos); if (p) p->sprite_id = g_sprite_ids? g_sprite_ids->try_get(it->sprite.empty()? it->name : it->sprite) : -1; }
                  }
                } else if (c < 0.85f && !dt.items.empty()) {
                  int t = pick_weighted(dt.items); if (t>=0) {
                    auto it = std::find_if(g_lua_mgr->items().begin(), g_lua_mgr->items().end(), [&](const ItemDef& d){ return d.type==t; });
                    if (it!=g_lua_mgr->items().end()) { Item itc{}; itc.active=true; itc.type=(std::uint32_t)it->type; itc.name=it->name; itc.count=1; itc.max_count=(std::uint32_t)it->max_count; itc.consume_on_use=it->consume_on_use; itc.category=it->category; itc.sprite_id = g_sprite_ids? g_sprite_ids->try_get(it->sprite.empty()? it->name : it->sprite) : -1; state.ground_items.spawn(itc, place_pos); }
                  }
                } else if (!dt.guns.empty()) {
                  int t = pick_weighted(dt.guns); if (t>=0) {
                    auto itg = std::find_if(g_lua_mgr->guns().begin(), g_lua_mgr->guns().end(), [&](const GunDef& g){ return g.type==t; });
                    if (itg!=g_lua_mgr->guns().end()) { int inst = state.guns.spawn_from_def(*itg); int gspr = g_sprite_ids? g_sprite_ids->try_get(itg->name) : -1; state.ground_guns.spawn(inst, place_pos, gspr); }
                  }
                }
              } else if (U(rng) < 0.5f && !g_lua_mgr->powerups().empty()) {
                std::uniform_int_distribution<int> di(0, (int)g_lua_mgr->powerups().size()-1);
                auto& pu = g_lua_mgr->powerups()[ (size_t)di(rng) ];
                auto* p = state.pickups.spawn((std::uint32_t)pu.type, pu.name, place_pos);
                if (p) p->sprite_id = sprites.try_get(pu.sprite.empty()? pu.name : pu.sprite);
              } else if (U(rng)<0.8f && !g_lua_mgr->items().empty()) {
                std::uniform_int_distribution<int> di(0, (int)g_lua_mgr->items().size()-1);
                auto idf = g_lua_mgr->items()[ (size_t)di(rng) ];
                Item it{}; it.active=true; it.type=(std::uint32_t)idf.type; it.name=idf.name; it.count=1; it.max_count=(std::uint32_t)idf.max_count; it.consume_on_use=idf.consume_on_use; it.category=idf.category; it.sprite_id = sprites.try_get(idf.sprite.empty()? idf.name : idf.sprite);
                state.ground_items.spawn(it, place_pos);
              } else if (!g_lua_mgr->guns().empty()) {
                std::uniform_int_distribution<int> di(0, (int)g_lua_mgr->guns().size()-1);
                auto gd = g_lua_mgr->guns()[ (size_t)di(rng) ];
                int inst = state.guns.spawn_from_def(gd);
                int gspr = g_sprite_ids? g_sprite_ids->try_get(gd.name) : -1; // allow name==sprite fallback
                state.ground_guns.spawn(inst, place_pos, gspr);
              }
            }
          }
        }
      }

      // on_tick triggers for items (player only) per fixed step
      if (state.player_vid && g_lua_mgr) {
        auto* pl = state.entities.get_mut(*state.player_vid);
        if (pl) {
          for (const auto& entry : state.inventory.entries) {
            g_lua_mgr->call_item_on_tick((int)entry.item.type, state, *pl, TIMESTEP);
          }
        }
      }
    }

    // Allow proceeding from score review after delay
    if (state.mode == ids::MODE_SCORE_REVIEW && state.score_ready_timer <= 0.0f) {
      if (state.menu_inputs.confirm || state.playing_inputs.use_center) {
        std::printf("[room] Proceeding to next area.\n");
        state.alerts.push_back({"Entering next area", 0.0f, 2.0f, false});
        state.mode = ids::MODE_PLAYING;
        generate_room(state, projectiles, renderer, gfx);
      }
    }

    // Paint a background every frame
    if (renderer)
    {
      SDL_SetRenderDrawColor(renderer, 18, 18, 20, 255); // dark gray
      SDL_RenderClear(renderer);
      // Fetch output size each frame
      SDL_GetRendererOutputSize(renderer, &width, &height);
      auto world_to_screen = [&](float wx, float wy) -> SDL_FPoint {
        float scale = TILE_SIZE * gfx.play_cam.zoom;
        float sx = (wx - gfx.play_cam.pos.x) * scale + static_cast<float>(width) * 0.5f;
        float sy = (wy - gfx.play_cam.pos.y) * scale + static_cast<float>(height) * 0.5f;
        return SDL_FPoint{sx, sy};
      };

      // draw tiles
      for (int y=0;y<(int)state.stage.get_height();++y) {
        for (int x=0;x<(int)state.stage.get_width();++x) {
          const auto& t = state.stage.at(x,y);
          bool is_start = (x == state.start_tile.x && y == state.start_tile.y);
          bool is_exit = (x == state.exit_tile.x && y == state.exit_tile.y);
          if (t.blocks_entities() || t.blocks_projectiles() || is_start || is_exit) {
            if (is_start) SDL_SetRenderDrawColor(renderer, 80, 220, 90, 255);
            else if (is_exit) SDL_SetRenderDrawColor(renderer, 240, 220, 80, 255);
            else if (t.blocks_entities() && t.blocks_projectiles()) SDL_SetRenderDrawColor(renderer, 90, 90, 90, 255); // wall
            else if (t.blocks_entities() && !t.blocks_projectiles()) SDL_SetRenderDrawColor(renderer, 70, 90, 160, 255); // void/water
            else SDL_SetRenderDrawColor(renderer, 90, 90, 90, 255);
            SDL_FPoint p0 = world_to_screen(static_cast<float>(x), static_cast<float>(y));
            float scale = TILE_SIZE * gfx.play_cam.zoom;
            SDL_Rect tr{ (int)std::floor(p0.x), (int)std::floor(p0.y), (int)std::ceil(scale), (int)std::ceil(scale) };
            SDL_RenderFillRect(renderer, &tr);
          }
        }
      }
      // draw entities
      for (auto const& e : state.entities.data()) {
        if (!e.active) continue;
        // sprite if available
        bool drew_sprite = false;
        if (e.sprite_id >= 0) {
          SDL_Texture* tex = textures.get(e.sprite_id);
          if (tex) {
            int tw=0, th=0; SDL_QueryTexture(tex, nullptr, nullptr, &tw, &th);
            SDL_FPoint c = world_to_screen(e.pos.x - e.size.x*0.5f, e.pos.y - e.size.y*0.5f);
            float scale = TILE_SIZE * gfx.play_cam.zoom;
            SDL_Rect dst{ (int)std::floor(c.x), (int)std::floor(c.y), (int)std::ceil(e.size.x*scale), (int)std::ceil(e.size.y*scale) };
            SDL_RenderCopy(renderer, tex, nullptr, &dst);
            drew_sprite = true;
          }
        }
        // debug AABB
        if (!drew_sprite) {
          if (e.type_ == ids::ET_PLAYER) SDL_SetRenderDrawColor(renderer, 60, 140, 240, 255);
          else if (e.type_ == ids::ET_NPC) SDL_SetRenderDrawColor(renderer, 220, 60, 60, 255);
          else SDL_SetRenderDrawColor(renderer, 180, 180, 200, 255);
          SDL_FPoint c = world_to_screen(e.pos.x - e.size.x*0.5f, e.pos.y - e.size.y*0.5f);
          float scale = TILE_SIZE * gfx.play_cam.zoom;
          SDL_Rect r{ (int)std::floor(c.x), (int)std::floor(c.y), (int)std::ceil(e.size.x*scale), (int)std::ceil(e.size.y*scale) };
          SDL_RenderFillRect(renderer, &r);
        } else {
          // draw outline AABB for debug
          SDL_SetRenderDrawColor(renderer, 255, 255, 255, 60);
          SDL_FPoint c = world_to_screen(e.pos.x - e.size.x*0.5f, e.pos.y - e.size.y*0.5f);
          float scale = TILE_SIZE * gfx.play_cam.zoom;
          SDL_Rect r{ (int)std::floor(c.x), (int)std::floor(c.y), (int)std::ceil(e.size.x*scale), (int)std::ceil(e.size.y*scale) };
          SDL_RenderDrawRect(renderer, &r);
        }
      }

      // draw pickups (power-ups) and ground items
      // pickups: green; items: cyan; optional AABB debug when overlapping player
      const Entity* pdraw = (state.player_vid? state.entities.get(*state.player_vid) : nullptr);
      glm::vec2 ph{0.0f}; if (pdraw) ph = pdraw->half_size();
      float pl=0,pr=0,pt=0,pb=0; if (pdraw){ pl=pdraw->pos.x-ph.x; pr=pdraw->pos.x+ph.x; pt=pdraw->pos.y-ph.y; pb=pdraw->pos.y+ph.y; }
      for (auto const& pu : state.pickups.data()) if (pu.active) {
        SDL_FPoint c = world_to_screen(pu.pos.x - 0.125f, pu.pos.y - 0.125f);
        float s = TILE_SIZE * gfx.play_cam.zoom * 0.25f;
        SDL_Rect r{ (int)std::floor(c.x), (int)std::floor(c.y), (int)std::ceil(s), (int)std::ceil(s) };
        if (pu.sprite_id >= 0) {
          if (SDL_Texture* tex = textures.get(pu.sprite_id)) SDL_RenderCopy(renderer, tex, nullptr, &r);
        } else {
          SDL_SetRenderDrawColor(renderer, 100, 220, 120, 255);
          SDL_RenderFillRect(renderer, &r);
        }
        if (pdraw) {
          float gl = pu.pos.x - 0.125f, gr = pu.pos.x + 0.125f;
          float gt = pu.pos.y - 0.125f, gb = pu.pos.y + 0.125f;
          bool overlap = !(pr <= gl || pl >= gr || pb <= gt || pt >= gb);
          if (overlap) { SDL_SetRenderDrawColor(renderer, 240, 220, 80, 255); SDL_RenderDrawRect(renderer, &r); }
        }
      }
      for (auto const& gi : state.ground_items.data()) if (gi.active) {
        SDL_FPoint c = world_to_screen(gi.pos.x - gi.size.x*0.5f, gi.pos.y - gi.size.y*0.5f);
        float scale = TILE_SIZE * gfx.play_cam.zoom;
        SDL_Rect r{ (int)std::floor(c.x), (int)std::floor(c.y), (int)std::ceil(gi.size.x*scale), (int)std::ceil(gi.size.y*scale) };
        if (gi.item.sprite_id >= 0) {
          if (SDL_Texture* tex = textures.get(gi.item.sprite_id)) SDL_RenderCopy(renderer, tex, nullptr, &r);
        } else {
          SDL_SetRenderDrawColor(renderer, 80, 220, 240, 255);
          SDL_RenderFillRect(renderer, &r);
        }
        if (pdraw) {
          glm::vec2 gh = gi.size*0.5f; float gl = gi.pos.x-gh.x, gr = gi.pos.x+gh.x; float gt = gi.pos.y-gh.y, gb = gi.pos.y+gh.y;
          bool overlap = !(pr <= gl || pl >= gr || pb <= gt || pt >= gb);
          if (overlap) {
            SDL_SetRenderDrawColor(renderer, 240, 220, 80, 255); SDL_RenderDrawRect(renderer, &r);
            if (ui_font) {
              std::string prompt = std::string("Press F to pick up ") + gi.item.name;
              SDL_Color col{250,250,250,255}; SDL_Surface* s = TTF_RenderUTF8_Blended(ui_font, prompt.c_str(), col);
              if (s){ SDL_Texture* t=SDL_CreateTextureFromSurface(renderer,s); int tw=0,th=0; SDL_QueryTexture(t,nullptr,nullptr,&tw,&th); SDL_Rect d{ r.x, r.y - th - 2, tw, th }; SDL_RenderCopy(renderer, t, nullptr, &d); SDL_DestroyTexture(t); SDL_FreeSurface(s);}            
            }
          }
        }
      }
      // draw ground guns (magenta if no sprite)
      for (auto const& gg : state.ground_guns.data()) if (gg.active) {
        SDL_FPoint c = world_to_screen(gg.pos.x - gg.size.x*0.5f, gg.pos.y - gg.size.y*0.5f);
        float scale = TILE_SIZE * gfx.play_cam.zoom;
        SDL_Rect r{ (int)std::floor(c.x), (int)std::floor(c.y), (int)std::ceil(gg.size.x*scale), (int)std::ceil(gg.size.y*scale) };
        if (gg.sprite_id >= 0) {
          if (SDL_Texture* tex = textures.get(gg.sprite_id)) SDL_RenderCopy(renderer, tex, nullptr, &r);
        } else {
          SDL_SetRenderDrawColor(renderer, 220, 120, 220, 255);
          SDL_RenderFillRect(renderer, &r);
        }
      }
      // draw projectiles
      SDL_SetRenderDrawColor(renderer, 240, 80, 80, 255);
      for (auto const& proj : projectiles.items) if (proj.active) {
        SDL_FPoint c = world_to_screen(proj.pos.x - proj.size.x*0.5f, proj.pos.y - proj.size.y*0.5f);
        float scale = TILE_SIZE * gfx.play_cam.zoom;
        SDL_Rect r{ (int)std::floor(c.x), (int)std::floor(c.y), (int)std::ceil(proj.size.x*scale), (int)std::ceil(proj.size.y*scale) };
        SDL_RenderFillRect(renderer, &r);
      }

      // draw cursor crosshair + circle (in screen-space at actual mouse position)
      {
        int mx = state.mouse_inputs.pos.x;
        int my = state.mouse_inputs.pos.y;
        SDL_SetRenderDrawColor(renderer, 250, 250, 250, 220);
        int cross = 8;
        SDL_RenderDrawLine(renderer, mx-cross, my, mx+cross, my);
        SDL_RenderDrawLine(renderer, mx, my-cross, mx, my+cross);
        // circle approximation
        int radius = 12;
        const int segments = 32;
        float prevx = static_cast<float>(mx) + static_cast<float>(radius);
        float prevy = (float)my;
        for (int i=1;i<=segments;++i) {
          float ang = static_cast<float>(i) * (2.0f * 3.14159265358979323846f / static_cast<float>(segments));
          float x = static_cast<float>(mx) + std::cos(ang) * static_cast<float>(radius);
          float y = static_cast<float>(my) + std::sin(ang) * static_cast<float>(radius);
          SDL_RenderDrawLine(renderer, (int)prevx, (int)prevy, (int)x, (int)y);
          prevx = x; prevy = y;
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
        int fgw = (int)std::lround(static_cast<double>(bar_w) * static_cast<double>(1.0f - ratio));
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
          SDL_Color lc{240,220,80,255};
          SDL_Surface* lsurf = TTF_RenderUTF8_Blended(ui_font, label, lc);
          if (lsurf) {
            SDL_Texture* ltex = SDL_CreateTextureFromSurface(renderer, lsurf);
            if (ltex) {
              int lw=0, lh=0; SDL_QueryTexture(ltex, nullptr, nullptr, &lw, &lh);
              SDL_Rect ldst{ bar_x, bar_y - lh - 6, lw, lh };
              SDL_RenderCopy(renderer, ltex, nullptr, &ldst);
              SDL_DestroyTexture(ltex);
            }
            SDL_FreeSurface(lsurf);
          }
          char txt[16];
          float secs = std::max(0.0f, state.exit_countdown);
          std::snprintf(txt, sizeof(txt), "%.1f", (double)secs);
          SDL_Color color{255,255,255,255};
          SDL_Surface* surf = TTF_RenderUTF8_Blended(ui_font, txt, color);
          if (surf) {
            SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surf);
            if (tex) {
              int tw=0, th=0; SDL_QueryTexture(tex, nullptr, nullptr, &tw, &th);
              SDL_Rect dst{ bar_x + bar_w/2 - tw/2, bar_y - th - 4, tw, th };
              SDL_RenderCopy(renderer, tex, nullptr, &dst);
              SDL_DestroyTexture(tex);
            }
            SDL_FreeSurface(surf);
          }
        }
      }

      // Score review overlay
      if (state.mode == ids::MODE_SCORE_REVIEW) {
        int box_w = width - 200;
        int box_h = 100;
        int box_x = (width - box_w) / 2;
        int box_y = 40;
        SDL_Rect box{box_x, box_y, box_w, box_h};
        SDL_SetRenderDrawColor(renderer, 30, 30, 40, 220);
        SDL_RenderFillRect(renderer, &box);
        SDL_SetRenderDrawColor(renderer, 200, 200, 220, 255);
        SDL_RenderDrawRect(renderer, &box);
        // Ready indicator bar if waiting
        if (state.score_ready_timer > 0.0f) {
          float ratio = state.score_ready_timer / SCORE_REVIEW_INPUT_DELAY;
          ratio = std::clamp(ratio, 0.0f, 1.0f);
          int wbw = (int)std::lround(static_cast<double>(box_w - 40) * static_cast<double>(ratio));
          SDL_Rect waitbar{box_x+20, box_y+box_h-24, wbw, 8};
          SDL_SetRenderDrawColor(renderer, 240, 220, 80, 220);
          SDL_RenderFillRect(renderer, &waitbar);
        }
        // Simple prompt line (no text rendering; draw a line accent)
        SDL_SetRenderDrawColor(renderer, 240, 220, 80, 255);
        SDL_RenderDrawLine(renderer, box_x+20, box_y+20, box_x+box_w-20, box_y+20);
      }

      // Character stats panel (left 30%, sliding)
      if (ui_font) {
        float target = state.show_character_panel ? 1.0f : 0.0f;
        // animate
        state.character_panel_slide = state.character_panel_slide + (target - state.character_panel_slide) * std::clamp((float)(dt_sec * 10.0), 0.0f, 1.0f);
        float panel_pct = 0.30f;
        int panel_w = (int)std::lround(static_cast<double>(width) * static_cast<double>(panel_pct));
        int px = (int)std::lround(-static_cast<double>(panel_w) * static_cast<double>(1.0f - state.character_panel_slide));
        SDL_Rect panel{ px, 0, panel_w, height };
        SDL_SetRenderDrawColor(renderer, 15, 15, 20, 220);
        SDL_RenderFillRect(renderer, &panel);
        SDL_SetRenderDrawColor(renderer, 240, 220, 80, 255);
        SDL_RenderDrawRect(renderer, &panel);
        // Stats text
        const Entity* p = (state.player_vid ? state.entities.get(*state.player_vid) : nullptr);
        if (p) {
          int tx = px + 16; int ty = 24; int lh = 18;
          auto draw_line = [&](const char* key, float val, const char* suffix){
            char buf[128]; std::snprintf(buf, sizeof(buf), "%s: %.1f%s", key, (double)val, suffix?suffix:"");
            SDL_Color c{220,220,220,255}; SDL_Surface* s=TTF_RenderUTF8_Blended(ui_font, buf, c);
            if (s){ SDL_Texture* t=SDL_CreateTextureFromSurface(renderer,s); int tw=0,th=0; SDL_QueryTexture(t,nullptr,nullptr,&tw,&th); SDL_Rect d{tx,ty,tw,th}; SDL_RenderCopy(renderer,t,nullptr,&d); SDL_DestroyTexture(t); SDL_FreeSurface(s);} ty += lh; };
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

      // Alerts list (top-left)
      if (ui_font) {
        int ax = 20; int ay = 90; int lh = 18;
        for (const auto& al : state.alerts) {
          SDL_Color c{ 200, 200, 220, 255 };
          SDL_Surface* s = TTF_RenderUTF8_Blended(ui_font, al.text.c_str(), c);
          if (s) { SDL_Texture* t = SDL_CreateTextureFromSurface(renderer, s); int tw=0,th=0; SDL_QueryTexture(t,nullptr,nullptr,&tw,&th); SDL_Rect d{ax, ay, tw, th}; SDL_RenderCopy(renderer, t, nullptr, &d); SDL_DestroyTexture(t); SDL_FreeSurface(s); }
          ay += lh;
        }
      }

      // Inventory list (left column under alerts) – hidden if character panel shown
      if (ui_font && !state.show_character_panel) {
        int sx = 40; int sy = 140; int slot_h = 26; int slot_w = 220;
        // Draw 10 slots
        for (int i=0;i<10;++i) {
          bool selected = (state.inventory.selected_index == (std::size_t)i);
          SDL_Rect slot{ sx, sy + i*slot_h, slot_w, slot_h - 6 };
          SDL_SetRenderDrawColor(renderer, selected ? 30 : 18, selected ? 30 : 18, selected ? 40 : 22, 200);
          SDL_RenderFillRect(renderer, &slot);
          SDL_SetRenderDrawColor(renderer, 60, 60, 70, 255);
          SDL_RenderDrawRect(renderer, &slot);
          // hotkey label
          char hk[4]; std::snprintf(hk, sizeof(hk), "%d", (i==9) ? 0 : (i+1));
          SDL_Color hotc{150,150,150,220}; SDL_Surface* hs=TTF_RenderUTF8_Blended(ui_font, hk, hotc);
          if (hs){ SDL_Texture* ht=SDL_CreateTextureFromSurface(renderer,hs); int tw=0,th=0; SDL_QueryTexture(ht,nullptr,nullptr,&tw,&th); SDL_Rect d{slot.x-20, slot.y+2, tw, th}; SDL_RenderCopy(renderer, ht, nullptr, &d); SDL_DestroyTexture(ht); SDL_FreeSurface(hs);}          
          // item text
          const InvEntry* ent = state.inventory.get((std::size_t)i);
          if (ent) {
            std::string ct = (ent->item.count>1) ? (" x" + std::to_string(ent->item.count)) : std::string("");
            std::string text = ent->item.name + ct;
            SDL_Color tc{230,230,230,255}; SDL_Surface* ts=TTF_RenderUTF8_Blended(ui_font, text.c_str(), tc);
            if (ts){ SDL_Texture* tt=SDL_CreateTextureFromSurface(renderer,ts); int tw=0,th=0; SDL_QueryTexture(tt,nullptr,nullptr,&tw,&th); SDL_Rect d{slot.x+8, slot.y+2, tw, th}; SDL_RenderCopy(renderer, tt, nullptr, &d); SDL_DestroyTexture(tt); SDL_FreeSurface(ts); }
          }
        }
      }

      // Right-side selected item details panel
      if (ui_font) {
        const InvEntry* sel = state.inventory.selected_entry();
        if (sel) {
          int panel_w = (int)std::lround(width * 0.26);
          int px = width - panel_w - 30;
          int py = (int)std::lround(height * 0.60);
          SDL_Rect box{px, py, panel_w, 200};
          SDL_SetRenderDrawColor(renderer, 25,25,30,220);
          SDL_RenderFillRect(renderer, &box);
          SDL_SetRenderDrawColor(renderer, 200,200,220,255);
          SDL_RenderDrawRect(renderer, &box);
          int tx = px + 12; int ty = py + 12; int lh = 18;
          auto draw_txt = [&](const std::string& s, SDL_Color col){ SDL_Surface* srf=TTF_RenderUTF8_Blended(ui_font, s.c_str(), col); if(srf){ SDL_Texture* t=SDL_CreateTextureFromSurface(renderer,srf); int tw=0,th=0; SDL_QueryTexture(t,nullptr,nullptr,&tw,&th); SDL_Rect d{tx,ty,tw,th}; SDL_RenderCopy(renderer,t,nullptr,&d); SDL_DestroyTexture(t); SDL_FreeSurface(srf);} ty += lh; };
          draw_txt(std::string("Selected: ") + sel->item.name, SDL_Color{255,255,255,255});
          draw_txt(std::string("Count: ") + std::to_string(sel->item.count) + "/" + std::to_string(sel->item.max_count), SDL_Color{220,220,220,255});
          // If item definition has a description from Lua, show it
          if (g_lua_mgr) {
            const ItemDef* idf = nullptr; for (auto const& d : g_lua_mgr->items()) if ((int)d.type == (int)sel->item.type) { idf=&d; break; }
            if (idf && !idf->desc.empty()) draw_txt(std::string("Desc: ") + idf->desc, SDL_Color{200,200,200,255});
          }
          if (sel->item.range > 0.0f) draw_txt(std::string("Range: ") + std::to_string((int)std::lround(sel->item.range)), SDL_Color{220,220,220,255});
          if (sel->item.use_cooldown > 0.0f) draw_txt(std::string("Cooldown: ") + std::to_string(sel->item.use_cooldown) + "s", SDL_Color{220,220,220,255});
          draw_txt(std::string("Consumable: ") + (sel->item.consume_on_use?"Yes":"No"), SDL_Color{220,220,220,255});
          draw_txt(std::string("Droppable: ") + (sel->item.droppable?"Yes":"No"), SDL_Color{220,220,220,255});
        }
      }

      // Right-side gun panel (player's equipped)
      if (ui_font && state.player_vid && g_lua_mgr) {
        const Entity* ply = state.entities.get(*state.player_vid);
        if (ply && ply->primary_gun_id >= 0) {
          // lookup gun def by type
          const GunDef* gd = nullptr;
          for (auto const& g : g_lua_mgr->guns()) if (g.type == ply->primary_gun_id) { gd = &g; break; }
          if (gd) {
            int panel_w = (int)std::lround(width * 0.26);
            int px = width - panel_w - 30;
            int py = (int)std::lround(height * 0.30);
            SDL_Rect box{px, py, panel_w, 180};
            SDL_SetRenderDrawColor(renderer, 25,25,30,220);
            SDL_RenderFillRect(renderer, &box);
            SDL_SetRenderDrawColor(renderer, 200,200,220,255);
            SDL_RenderDrawRect(renderer, &box);
            int tx = px + 12; int ty = py + 12; int lh = 18;
            auto draw_txt = [&](const std::string& s, SDL_Color col){ SDL_Surface* srf=TTF_RenderUTF8_Blended(ui_font, s.c_str(), col); if(srf){ SDL_Texture* t=SDL_CreateTextureFromSurface(renderer,srf); int tw=0,th=0; SDL_QueryTexture(t,nullptr,nullptr,&tw,&th); SDL_Rect d{tx,ty,tw,th}; SDL_RenderCopy(renderer,t,nullptr,&d); SDL_DestroyTexture(t); SDL_FreeSurface(srf);} ty += lh; };
            draw_txt(std::string("Gun: ") + gd->name, SDL_Color{255,255,255,255});
            draw_txt(std::string("Damage: ") + std::to_string((int)std::lround(gd->damage)), SDL_Color{220,220,220,255});
            draw_txt(std::string("RPM: ") + std::to_string((int)std::lround(gd->rpm)), SDL_Color{220,220,220,255});
            draw_txt(std::string("Recoil: ") + std::to_string(gd->recoil), SDL_Color{220,220,220,255});
            draw_txt(std::string("Control: ") + std::to_string(gd->control), SDL_Color{220,220,220,255});
          // Show instance ammo if available
          if (state.player_vid) {
            const Entity* ply2 = state.entities.get(*state.player_vid);
            const GunInstance* gi = (ply2 && ply2->primary_gun_inst>=0) ? state.guns.get(ply2->primary_gun_inst) : nullptr;
            if (gi) {
              draw_txt(std::string("Mag: ") + std::to_string(gi->current_mag) + std::string(" / Reserve: ") + std::to_string(gi->ammo_reserve), SDL_Color{220,220,220,255});
            } else {
              draw_txt(std::string("Mag: ") + std::to_string(gd->mag) + std::string(" / Ammo: ") + std::to_string(gd->ammo_max), SDL_Color{220,220,220,255});
            }
          }
          }
        }
      }

      SDL_RenderPresent(renderer);
    }
    else
    {
      SDL_Delay(16);
    }

    // FPS calculation using high-resolution timer
    // dt_sec computed above
    accum_sec += dt_sec;
    frame_counter += 1;
    if (accum_sec >= 1.0)
    {
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
      if (--arg_frames <= 0) running = false;
    }
  }

  if (renderer)
  {
    SDL_DestroyRenderer(renderer);
  }
  SDL_DestroyWindow(window);
  if (ui_font) TTF_CloseFont(ui_font);
  if (TTF_WasInit()) TTF_Quit();
  SDL_Quit();
  return 0;
}
static void generate_room(State& state, Projectiles& projectiles, SDL_Renderer* renderer, Graphics& gfx) {
  // Reset world
  projectiles = Projectiles{};
  state.entities = Entities{};
  state.player_vid.reset();
  state.start_tile = {-1,-1};
  state.exit_tile = {-1,-1};
  state.exit_countdown = -1.0f;
  state.score_ready_timer = 0.0f;
  state.pickups.clear();
  state.ground_items.clear();
  // Rebuild stage
  // Random dimensions between 32 and 64
  std::mt19937 rng{std::random_device{}()};
  std::uniform_int_distribution<int> dwh(32, 64);
  std::uint32_t W = static_cast<std::uint32_t>(dwh(rng));
  std::uint32_t H = static_cast<std::uint32_t>(dwh(rng));
  state.stage = Stage(W, H);
  state.stage.fill_border(TileProps::Make(true, true));
  // sprinkle obstacles (walls and voids)
  int tiles = static_cast<int>(W*H);
  int obstacles = tiles / 8; // ~12.5%
  std::uniform_int_distribution<int> dx(1, static_cast<int>(W)-2);
  std::uniform_int_distribution<int> dy(1, static_cast<int>(H)-2);
  std::uniform_int_distribution<int> type(0, 3); // 0..1 void (blocks entities only), 2..3 wall (blocks both)

  // Determine start and exit corners (inside border)
  std::vector<glm::ivec2> corners = {
    {1,1}, {static_cast<int>(W)-2,1}, {1, static_cast<int>(H)-2}, {static_cast<int>(W)-2, static_cast<int>(H)-2}
  };
  // Pick first two distinct non-block corners
  int start_idx = -1, exit_idx = -1;
  for (int i=0;i<(int)corners.size();++i) {
    auto c = corners[static_cast<size_t>(i)];
    if (state.stage.in_bounds(c.x, c.y) && !state.stage.at(c.x,c.y).blocks_entities()) { start_idx = i; break; }
  }
  for (int i=(int)corners.size()-1;i>=0;--i) {
    if (i==start_idx) continue;
    auto c = corners[static_cast<size_t>(i)];
    if (state.stage.in_bounds(c.x, c.y) && !state.stage.at(c.x,c.y).blocks_entities()) { exit_idx = i; break; }
  }
  if (start_idx < 0) { start_idx = 0; auto c=corners[static_cast<size_t>(0)]; state.stage.at(c.x,c.y) = TileProps::Make(false,false); }
  if (exit_idx < 0 || exit_idx == start_idx) { exit_idx = (start_idx+3)%4; auto c=corners[static_cast<size_t>(exit_idx)]; state.stage.at(c.x,c.y) = TileProps::Make(false,false); }

  state.start_tile = corners[static_cast<size_t>(start_idx)];
  state.exit_tile = corners[static_cast<size_t>(exit_idx)];
  // Place obstacles now, avoiding start/exit tiles
  for (int i = 0; i < obstacles; ++i) {
    int x = dx(rng);
    int y = dy(rng);
    if ((x == state.start_tile.x && y == state.start_tile.y) || (x == state.exit_tile.x && y == state.exit_tile.y)) {
      continue;
    }
    int t = type(rng);
    if (t <= 1) {
      state.stage.at(x,y) = TileProps::Make(true, false); // void/water: blocks entities only
    } else {
      state.stage.at(x,y) = TileProps::Make(true, true); // wall: blocks both
    }
  }

  // Create player at start
  if (auto pvid = state.entities.new_entity()) {
    Entity* p = state.entities.get_mut(*pvid);
    p->type_ = ids::ET_PLAYER;
    p->size = {0.25f, 0.25f};
    p->pos = {static_cast<float>(state.start_tile.x)+0.5f, static_cast<float>(state.start_tile.y)+0.5f};
    if (g_sprite_ids) p->sprite_id = g_sprite_ids->try_get("player");
    state.player_vid = pvid;
  }

  // Spawn some NPCs
  {
    std::mt19937 rng2{std::random_device{}()};
    std::uniform_int_distribution<int> dx2(1, (int)state.stage.get_width()-2);
    std::uniform_int_distribution<int> dy2(1, (int)state.stage.get_height()-2);
    for (int i=0;i<25;++i) {
      auto vid = state.entities.new_entity();
      if (!vid) break;
      Entity* e = state.entities.get_mut(*vid);
      e->type_ = ids::ET_NPC;
      e->size = {0.25f, 0.25f};
      if (g_sprite_ids) e->sprite_id = g_sprite_ids->try_get("zombie");
      for (int tries=0; tries<100; ++tries) {
        int x = dx2(rng2), y = dy2(rng2);
        if (!state.stage.at(x,y).blocks_entities()) { e->pos = {static_cast<float>(x)+0.5f, static_cast<float>(y)+0.5f}; break; }
      }
    }
  }

  // Camera to player and zoom for ~8%
  if (renderer && state.player_vid) {
    int ww=0, wh=0; SDL_GetRendererOutputSize(renderer, &ww, &wh);
    float min_dim = static_cast<float>(std::min(ww, wh));
    const Entity* p = state.entities.get(*state.player_vid);
    if (p) {
      float desired_px = 0.08f * min_dim;
      float zoom = desired_px / (p->size.y * TILE_SIZE);
      zoom = std::clamp(zoom, 0.5f, 32.0f);
      gfx.play_cam.zoom = zoom;
      gfx.play_cam.pos = p->pos;
    }
  }

  // Spawn a few Lua-defined pickups/items near start for testing
  {
    glm::vec2 base = { static_cast<float>(state.start_tile.x)+0.5f, static_cast<float>(state.start_tile.y)+0.5f };
    auto place = [&](glm::vec2 offs){ return ensure_not_in_block(state, base + offs); };
    if (g_lua_mgr && !g_lua_mgr->powerups().empty()) {
      auto& pu = g_lua_mgr->powerups()[0];
      auto* p = state.pickups.spawn(static_cast<std::uint32_t>(pu.type), pu.name, place({1.0f, 0.0f}));
      if (p && g_sprite_ids) p->sprite_id = g_sprite_ids->try_get(pu.sprite.empty()? pu.name : pu.sprite);
    }
    if (g_lua_mgr && g_lua_mgr->powerups().size()>1) {
      auto& pu = g_lua_mgr->powerups()[1];
      auto* p = state.pickups.spawn(static_cast<std::uint32_t>(pu.type), pu.name, place({0.0f, 1.0f}));
      if (p && g_sprite_ids) p->sprite_id = g_sprite_ids->try_get(pu.sprite.empty()? pu.name : pu.sprite);
    }
    if (g_lua_mgr && !g_lua_mgr->items().empty()) {
      auto id = g_lua_mgr->items()[0]; Item it{}; it.active=true; it.type=static_cast<std::uint32_t>(id.type); it.name=id.name; it.count=1; it.max_count=static_cast<std::uint32_t>(id.max_count); it.consume_on_use=id.consume_on_use; it.category=id.category; it.sprite_id = g_sprite_ids? g_sprite_ids->try_get(id.sprite.empty()? id.name : id.sprite) : -1; state.ground_items.spawn(it, place({2.0f, 0.0f}));
    }
    if (g_lua_mgr && g_lua_mgr->items().size()>1) {
      auto id = g_lua_mgr->items()[1]; Item it{}; it.active=true; it.type=static_cast<std::uint32_t>(id.type); it.name=id.name; it.count=1; it.max_count=static_cast<std::uint32_t>(id.max_count); it.consume_on_use=id.consume_on_use; it.category=id.category; it.sprite_id = g_sprite_ids? g_sprite_ids->try_get(id.sprite.empty()? id.name : id.sprite) : -1; state.ground_items.spawn(it, place({0.0f, 2.0f}));
    }
  }
}
static bool tile_blocks_entity(const State& state, int x, int y) {
  return !state.stage.in_bounds(x,y) || state.stage.at(x,y).blocks_entities();
}

static glm::ivec2 nearest_walkable_tile(const State& state, glm::ivec2 t, int max_radius=8) {
  if (!tile_blocks_entity(state, t.x, t.y)) return t;
  for (int r=1; r<=max_radius; ++r) {
    for (int dy=-r; dy<=r; ++dy) {
      int y = t.y + dy;
      int dx = r - std::abs(dy);
      for (int sx : {-dx, dx}) {
        int x = t.x + sx;
        if (!tile_blocks_entity(state, x, y)) return {x,y};
      }
    }
  }
  return t;
}

static glm::vec2 ensure_not_in_block(const State& state, glm::vec2 pos) {
  glm::ivec2 t{ (int)std::floor(pos.x), (int)std::floor(pos.y) };
  glm::ivec2 w = nearest_walkable_tile(state, t, 16);
  if (w != t) return glm::vec2{ (float)w.x + 0.5f, (float)w.y + 0.5f };
  return pos;
}
