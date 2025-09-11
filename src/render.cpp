#include "render.hpp"

#include "globals.hpp"
#include "tex.hpp"
#include "audio.hpp"
#include "luamgr.hpp"
#include "sprites.hpp"
#include "settings.hpp"
#include "sound.hpp"

#include <algorithm>
#include <cmath>
#include <random>
#include <string>
#include <vector>

namespace {
static inline std::string fmt2(float v) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.2f", (double)v);
    return std::string(buf);
}
static inline void ui_draw_kv_line(SDL_Renderer* renderer, TTF_Font* font,
                                   int tx, int& ty, int lh,
                                   const char* key, const std::string& value,
                                   SDL_Color key_color = SDL_Color{150,150,150,255},
                                   SDL_Color val_color = SDL_Color{230,230,230,255}) {
    SDL_Surface* ks = TTF_RenderUTF8_Blended(font, (std::string(key)+": ").c_str(), key_color);
    int keyw=0,keyh=0; SDL_Texture* kt=nullptr;
    if (ks) { kt = SDL_CreateTextureFromSurface(renderer, ks);
        SDL_QueryTexture(kt,nullptr,nullptr,&keyw,&keyh);
        SDL_Rect kd{tx, ty, keyw, keyh}; SDL_RenderCopy(renderer, kt, nullptr, &kd);
        SDL_DestroyTexture(kt); SDL_FreeSurface(ks);
    }
    SDL_Surface* vs = TTF_RenderUTF8_Blended(font, value.c_str(), val_color);
    if (vs) { SDL_Texture* vt = SDL_CreateTextureFromSurface(renderer, vs);
        int vw=0,vh=0; SDL_QueryTexture(vt,nullptr,nullptr,&vw,&vh);
        SDL_Rect vd{tx+keyw, ty, vw, vh}; SDL_RenderCopy(renderer, vt, nullptr, &vd);
        SDL_DestroyTexture(vt); SDL_FreeSurface(vs);
    }
    ty += lh;
}
}

void render_frame(Graphics& gfx,
                  double dt_sec,
                  const Projectiles& projectiles) {
    auto& state = *g_state;
    SDL_Renderer* renderer = gfx.renderer;
    if (!renderer) {
        SDL_Delay(16);
        return;
    }

    int width = 0, height = 0;
    SDL_SetRenderDrawColor(renderer, 18, 18, 20, 255); // dark gray
    SDL_RenderClear(renderer);
    // window is available via gfx.window if needed

    // One-frame warnings (e.g., missing sprites), rendered in red.
    std::vector<std::string> frame_warnings;
    auto add_warning = [&](const std::string& s) {
        if (std::find(frame_warnings.begin(), frame_warnings.end(), s) == frame_warnings.end())
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
                    SDL_FPoint p0 = world_to_screen(static_cast<float>(x), static_cast<float>(y));
                    float scale = TILE_SIZE * gfx.play_cam.zoom;
                    SDL_Rect tr{(int)std::floor(p0.x), (int)std::floor(p0.y),
                                (int)std::ceil(scale), (int)std::ceil(scale)};
                    SDL_RenderFillRect(renderer, &tr);
                }
            }
        }
    }

    // Draw crates (visuals only); open progression computed in sim
    if (state.mode == ids::MODE_PLAYING) {
        for (auto& c : state.crates.data())
            if (c.active && !c.opened) {
                glm::vec2 ch = c.size * 0.5f;
                // world → screen rect
                SDL_FPoint c0 = world_to_screen(c.pos.x - ch.x, c.pos.y - ch.y);
                float scale = TILE_SIZE * gfx.play_cam.zoom;
                SDL_Rect rc{(int)std::floor(c0.x), (int)std::floor(c0.y),
                            (int)std::ceil(c.size.x * scale), (int)std::ceil(c.size.y * scale)};
                SDL_SetRenderDrawColor(renderer, 120, 80, 40, 255);
                SDL_RenderFillRect(renderer, &rc);
                SDL_SetRenderDrawColor(renderer, 200, 160, 100, 255);
                SDL_RenderDrawRect(renderer, &rc);
                // label above
                if (gfx.ui_font && g_lua_mgr) {
                    std::string label = "Crate";
                    if (auto const* cd = g_lua_mgr->find_crate(c.def_type))
                        label = cd->label.empty() ? cd->name : cd->label;
                    SDL_Color lc{240, 220, 80, 255};
                    SDL_Surface* lsrf = TTF_RenderUTF8_Blended(gfx.ui_font, label.c_str(), lc);
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
                int fw = (int)std::lround((double)bw * (double)(c.open_progress / std::max(0.0001f, open_time)));
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
                SDL_Texture* tex = (g_textures ? g_textures->get(e.sprite_id) : nullptr);
                if (tex) {
                    int tw = 0, th = 0;
                    SDL_QueryTexture(tex, nullptr, nullptr, &tw, &th);
                    SDL_FPoint c = world_to_screen(e.pos.x - e.size.x * 0.5f, e.pos.y - e.size.y * 0.5f);
                    float scale = TILE_SIZE * gfx.play_cam.zoom;
                    SDL_Rect dst{(int)std::floor(c.x), (int)std::floor(c.y),
                                 (int)std::ceil(e.size.x * scale), (int)std::ceil(e.size.y * scale)};
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
                SDL_FPoint c = world_to_screen(e.pos.x - e.size.x * 0.5f, e.pos.y - e.size.y * 0.5f);
                float scale = TILE_SIZE * gfx.play_cam.zoom;
                SDL_Rect r{(int)std::floor(c.x), (int)std::floor(c.y),
                           (int)std::ceil(e.size.x * scale), (int)std::ceil(e.size.y * scale)};
                SDL_RenderFillRect(renderer, &r);
            } else {
                // draw outline AABB for debug
                SDL_SetRenderDrawColor(renderer, 255, 255, 255, 60);
                SDL_FPoint c = world_to_screen(e.pos.x - e.size.x * 0.5f, e.pos.y - e.size.y * 0.5f);
                float scale = TILE_SIZE * gfx.play_cam.zoom;
                SDL_Rect r{(int)std::floor(c.x), (int)std::floor(c.y),
                           (int)std::ceil(e.size.x * scale), (int)std::ceil(e.size.y * scale)};
                SDL_RenderDrawRect(renderer, &r);
            }
            // Player held gun: rotate sprite around player towards mouse
            if (e.type_ == ids::ET_PLAYER) {
                const Entity* pl = &e;
                if (pl->equipped_gun_vid.has_value() && g_lua_mgr) {
                    const GunInstance* gi = state.guns.get(*pl->equipped_gun_vid);
                    const GunDef* gd = nullptr;
                    if (gi) {
                        for (auto const& g : g_lua_mgr->guns()) if (g.type == gi->def_type) { gd = &g; break; }
                    }
                    int gspr = -1;
                    if (gd && g_sprite_ids) {
                        if (!gd->sprite.empty() && gd->sprite.find(':') != std::string::npos)
                            gspr = g_sprite_ids->try_get(gd->sprite);
                    }
                    // Compute aim angle and world position under mouse
                    int ww = width, wh = height;
                    SDL_GetRendererOutputSize(renderer, &ww, &wh);
                    float inv_scale = 1.0f / (TILE_SIZE * gfx.play_cam.zoom);
                    glm::vec2 m = {
                        gfx.play_cam.pos.x + (static_cast<float>(state.mouse_inputs.pos.x) - (float)ww * 0.5f) * inv_scale,
                        gfx.play_cam.pos.y + (static_cast<float>(state.mouse_inputs.pos.y) - (float)wh * 0.5f) * inv_scale};
                    glm::vec2 dir = glm::normalize(m - pl->pos);
                    if (glm::any(glm::isnan(dir))) dir = {1.0f, 0.0f};
                    float angle_deg = std::atan2(dir.y, dir.x) * 180.0f / 3.14159265f;
                    glm::vec2 gun_pos = pl->pos + dir * GUN_HOLD_OFFSET_UNITS;
                    SDL_FPoint c0 = world_to_screen(gun_pos.x - 0.15f, gun_pos.y - 0.10f);
                    float scale = TILE_SIZE * gfx.play_cam.zoom;
                    SDL_Rect r{(int)std::floor(c0.x), (int)std::floor(c0.y), (int)std::ceil(0.30f * scale), (int)std::ceil(0.20f * scale)};
                    if (gspr >= 0) {
                        if (SDL_Texture* tex = (g_textures ? g_textures->get(gspr) : nullptr))
                            SDL_RenderCopyEx(renderer, tex, nullptr, &r, angle_deg, nullptr, SDL_FLIP_NONE);
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
    if (state.mode == ids::MODE_PLAYING) {
        for (auto const& e : state.entities.data()) {
            if (!e.active || e.type_ != ids::ET_NPC)
                continue;
            // Always show bars; if max_hp is zero, skip HP bar but keep slivers if any
            SDL_FPoint c = world_to_screen(e.pos.x - e.size.x * 0.5f, e.pos.y - e.size.y * 0.5f);
            float scale = TILE_SIZE * gfx.play_cam.zoom;
            int w = (int)std::ceil(e.size.x * scale);
            int h = 6;
            SDL_Rect bg{(int)std::floor(c.x), (int)std::floor(c.y) - (h + 4), w, h};
            SDL_SetRenderDrawColor(renderer, 30, 30, 34, 220);
            SDL_RenderFillRect(renderer, &bg);
            SDL_SetRenderDrawColor(renderer, 80, 80, 90, 255);
            SDL_RenderDrawRect(renderer, &bg);
            if (e.max_hp > 0) {
                float ratio = (float)e.health / (float)e.max_hp;
                ratio = std::clamp(ratio, 0.0f, 1.0f);
                int hw = (int)std::lround((double)w * (double)ratio);
                SDL_Rect hr{bg.x, bg.y, hw, bg.h};
                SDL_SetRenderDrawColor(renderer, 220, 60, 60, 230);
                SDL_RenderFillRect(renderer, &hr);
            }
            // Optional shield indicator as thin cyan band just above the HP bar
            if (e.stats.shield_max > 0.0f && e.shield > 0.0f) {
                float sratio = std::clamp(e.shield / e.stats.shield_max, 0.0f, 1.0f);
                int sw = (int)std::lround((double)w * (double)sratio);
                int sh = 3;
                SDL_Rect sb{bg.x, bg.y - (sh + 2), sw, sh};
                SDL_SetRenderDrawColor(renderer, 120, 200, 240, 220);
                SDL_RenderFillRect(renderer, &sb);
            }
            // Plates as thin slivers aligned from right edge above the shield band
            if (e.stats.plates > 0) {
                int to_show = std::min(20, e.stats.plates);
                int slw = 3, gap = 1;
                int slh = 4;
                int py = bg.y - (slh + 6);
                for (int i = 0; i < to_show; ++i) {
                    int sx = bg.x + w - (i + 1) * (slw + gap);
                    SDL_Rect pr{sx, py, slw, slh};
                    SDL_SetRenderDrawColor(renderer, 80, 80, 80, 255);
                    SDL_RenderFillRect(renderer, &pr);
                    SDL_SetRenderDrawColor(renderer, 140, 140, 140, 255);
                    SDL_RenderDrawRect(renderer, &pr);
                }
            }
        }
    }

    // draw pickups (power-ups) and ground items
    if (state.mode == ids::MODE_PLAYING) {
        const Entity* pdraw = (state.player_vid ? state.entities.get(*state.player_vid) : nullptr);
        glm::vec2 ph{0.0f, 0.0f};
        float pl = 0, pr = 0, pt = 0, pb = 0;
        if (pdraw)
            ph = pdraw->half_size();
        if (pdraw) {
            pl = pdraw->pos.x - ph.x;
            pr = pdraw->pos.x + ph.x;
            pt = pdraw->pos.y - ph.y;
            pb = pdraw->pos.y + ph.y;
        }
        // draw powerups
        for (auto const& pu : state.pickups.data())
            if (pu.active) {
                SDL_FPoint c = world_to_screen(pu.pos.x - 0.125f, pu.pos.y - 0.125f);
                float scale = TILE_SIZE * gfx.play_cam.zoom;
                SDL_Rect r{(int)std::floor(c.x), (int)std::floor(c.y),
                           (int)std::ceil(0.25f * scale), (int)std::ceil(0.25f * scale)};
                int sid = pu.sprite_id;
                if (sid < 0 && g_lua_mgr && g_sprite_ids) {
                    for (auto const& pd : g_lua_mgr->powerups())
                        if ((int)pd.type == (int)pu.type) {
                            if (!pd.sprite.empty() && pd.sprite.find(':') != std::string::npos)
                                sid = g_sprite_ids->try_get(pd.sprite);
                            break;
                        }
                }
                if (sid >= 0) {
                    if (SDL_Texture* tex = (g_textures ? g_textures->get(sid) : nullptr))
                        SDL_RenderCopy(renderer, tex, nullptr, &r);
                    else
                        add_warning("Missing texture for powerup sprite");
                } else {
                    add_warning("Missing sprite for powerup");
                    SDL_SetRenderDrawColor(renderer, 100, 220, 120, 255);
                    SDL_RenderFillRect(renderer, &r);
                }
            }
        // draw ground items and guns; highlight overlaps and prompt
        enum class PK { None, Item, Gun };
        auto overlap_area = [&](float al, float at, float ar, float ab, float bl, float bt, float br, float bb) -> float {
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
        float best_area = 0.0f; PK best_kind = PK::None; std::size_t best_idx = (std::size_t)-1;
        // Ground items: draw and track best-overlap using index loop
        for (std::size_t i = 0; i < state.ground_items.data().size(); ++i) {
            auto const& gi = state.ground_items.data()[i];
            if (!gi.active) continue;
            SDL_FPoint c = world_to_screen(gi.pos.x - gi.size.x * 0.5f, gi.pos.y - gi.size.y * 0.5f);
            float scale = TILE_SIZE * gfx.play_cam.zoom;
            SDL_Rect r{(int)std::floor(c.x), (int)std::floor(c.y),
                       (int)std::ceil(gi.size.x * scale), (int)std::ceil(gi.size.y * scale)};
            int ispr = -1;
            if (g_lua_mgr) {
                const ItemInstance* inst = state.items.get(gi.item_vid);
                if (inst) {
                    for (auto const& d : g_lua_mgr->items())
                        if (d.type == inst->def_type) {
                            if (!d.sprite.empty() && d.sprite.find(':') != std::string::npos)
                                ispr = g_sprite_ids ? g_sprite_ids->try_get(d.sprite) : -1;
                            else
                                ispr = -1;
                            break;
                        }
                }
            }
            if (ispr >= 0) {
                if (SDL_Texture* tex = (g_textures ? g_textures->get(ispr) : nullptr))
                    SDL_RenderCopy(renderer, tex, nullptr, &r);
            } else {
                add_warning("Missing sprite for item");
                SDL_SetRenderDrawColor(renderer, 80, 220, 240, 255);
                SDL_RenderFillRect(renderer, &r);
            }
            if (pdraw) {
                glm::vec2 gh = gi.size * 0.5f;
                float gl = gi.pos.x - gh.x, gr = gi.pos.x + gh.x;
                float gt = gi.pos.y - gh.y, gb = gi.pos.y + gh.y;
                float area_i = overlap_area(pl, pt, pr, pb, gl, gt, gr, gb);
                if (area_i > best_area) { best_area = area_i; best_kind = PK::Item; best_idx = i; }
            }
        }
        // Ground guns
        for (std::size_t i = 0; i < state.ground_guns.data().size(); ++i) {
            auto const& gg = state.ground_guns.data()[i];
            if (!gg.active) continue;
            SDL_FPoint c = world_to_screen(gg.pos.x - gg.size.x * 0.5f, gg.pos.y - gg.size.y * 0.5f);
            float scale = TILE_SIZE * gfx.play_cam.zoom;
            SDL_Rect r{(int)std::floor(c.x), (int)std::floor(c.y),
                       (int)std::ceil(gg.size.x * scale), (int)std::ceil(gg.size.y * scale)};
            int sid = gg.sprite_id;
            if (sid < 0 && g_lua_mgr && g_sprite_ids) {
                if (const GunInstance* gi = state.guns.get(gg.gun_vid)) {
                    const GunDef* gd = nullptr;
                    for (auto const& g : g_lua_mgr->guns()) if (g.type == gi->def_type) { gd = &g; break; }
                    if (gd && !gd->sprite.empty() && gd->sprite.find(':') != std::string::npos)
                        sid = g_sprite_ids->try_get(gd->sprite);
                }
            }
            if (sid >= 0) {
                if (SDL_Texture* tex = (g_textures ? g_textures->get(sid) : nullptr))
                    SDL_RenderCopy(renderer, tex, nullptr, &r);
                else
                    add_warning("Missing texture for gun sprite");
            } else {
                add_warning("Missing sprite for gun");
                SDL_SetRenderDrawColor(renderer, 220, 120, 220, 255);
                SDL_RenderFillRect(renderer, &r);
            }
            if (pdraw) {
                glm::vec2 gh = gg.size * 0.5f;
                float gl = gg.pos.x - gh.x, gr = gg.pos.x + gh.x;
                float gt = gg.pos.y - gh.y, gb = gg.pos.y + gh.y;
                float area_g = overlap_area(pl, pt, pr, pb, gl, gt, gr, gb);
                if (area_g > best_area) { best_area = area_g; best_kind = PK::Gun; best_idx = i; }
            }
        }
        // Consolidated pickup prompt
        if (pdraw && gfx.ui_font && state.mode == ids::MODE_PLAYING && best_area > 0.0f) {
            std::string nm;
            SDL_Rect r{0, 0, 0, 0};
            if (best_kind == PK::Item) {
                auto const& gi = state.ground_items.data()[best_idx];
                SDL_FPoint c = world_to_screen(gi.pos.x - gi.size.x * 0.5f, gi.pos.y - gi.size.y * 0.5f);
                float scale = TILE_SIZE * gfx.play_cam.zoom;
                r = SDL_Rect{(int)std::floor(c.x), (int)std::floor(c.y),
                             (int)std::ceil(gi.size.x * scale), (int)std::ceil(gi.size.y * scale)};
                nm = "item";
                if (g_lua_mgr) {
                    const ItemInstance* inst = state.items.get(gi.item_vid);
                    if (inst) {
                        for (auto const& d : g_lua_mgr->items()) if (d.type == inst->def_type) { nm = d.name; break; }
                    }
                }
            } else if (best_kind == PK::Gun) {
                auto const& gg = state.ground_guns.data()[best_idx];
                SDL_FPoint c = world_to_screen(gg.pos.x - gg.size.x * 0.5f, gg.pos.y - gg.size.y * 0.5f);
                float scale = TILE_SIZE * gfx.play_cam.zoom;
                r = SDL_Rect{(int)std::floor(c.x), (int)std::floor(c.y),
                             (int)std::ceil(gg.size.x * scale), (int)std::ceil(gg.size.y * scale)};
                nm = "gun";
                if (g_lua_mgr) {
                    if (const GunInstance* gi = state.guns.get(gg.gun_vid)) {
                        for (auto const& g : g_lua_mgr->guns()) if (g.type == gi->def_type) { nm = g.name; break; }
                    }
                }
            }
            SDL_SetRenderDrawColor(renderer, 240, 220, 80, 255);
            SDL_RenderDrawRect(renderer, &r);
            const char* keyname = nullptr;
            if (g_binds) keyname = SDL_GetScancodeName(g_binds->pick_up);
            if (!keyname || !*keyname) keyname = "F";
            std::string prompt = std::string("Press ") + keyname + " to pick up " + nm;
            SDL_Color col{250, 250, 250, 255};
            SDL_Surface* s = TTF_RenderUTF8_Blended(gfx.ui_font, prompt.c_str(), col);
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
                if (best_kind == PK::Item) {
                    auto const& gi = state.ground_items.data()[best_idx];
                    const ItemInstance* inst = state.items.get(gi.item_vid);
                    std::string iname = "item";
                    std::string idesc; bool consume = false; int sid = -1;
                    if (g_lua_mgr && inst) {
                        for (auto const& ddf : g_lua_mgr->items()) if (ddf.type == inst->def_type) {
                            iname = ddf.name; idesc = ddf.desc; consume = ddf.consume_on_use;
                            if (!ddf.sprite.empty() && ddf.sprite.find(':') != std::string::npos && g_sprite_ids)
                                sid = g_sprite_ids->try_get(ddf.sprite);
                            break; }
                    }
                    if (sid >= 0 && g_textures) if (SDL_Texture* texi = g_textures->get(sid)) { SDL_Rect dst{tx, ty, 48, 32}; SDL_RenderCopy(renderer, texi, nullptr, &dst); ty += 36; }
                    ui_draw_kv_line(renderer, gfx.ui_font, tx, ty, lh, "Item", iname);
                    if (!idesc.empty()) ui_draw_kv_line(renderer, gfx.ui_font, tx, ty, lh, "Desc", idesc);
                    ui_draw_kv_line(renderer, gfx.ui_font, tx, ty, lh, "Consumable", consume ? std::string("Yes") : std::string("No"));
                } else if (best_kind == PK::Gun) {
                    auto const& gg = state.ground_guns.data()[best_idx];
                    const GunInstance* gim = state.guns.get(gg.gun_vid);
                    const GunDef* gdp = nullptr;
                    if (g_lua_mgr && gim) { for (auto const& gdf : g_lua_mgr->guns()) if (gdf.type == gim->def_type) { gdp = &gdf; break; } }
                    if (gdp) {
                        int gun_sid = -1; if (!gdp->sprite.empty() && g_sprite_ids) gun_sid = g_sprite_ids->try_get(gdp->sprite);
                        if (gun_sid >= 0 && g_textures) if (SDL_Texture* texg = g_textures->get(gun_sid)) { SDL_Rect dst{tx, ty, 64, 40}; SDL_RenderCopy(renderer, texg, nullptr, &dst); ty += 44; }
                        ui_draw_kv_line(renderer, gfx.ui_font, tx, ty, lh, "Gun", gdp->name);
                        ui_draw_kv_line(renderer, gfx.ui_font, tx, ty, lh, "Damage", std::to_string((int)std::lround(gdp->damage)));
                        ui_draw_kv_line(renderer, gfx.ui_font, tx, ty, lh, "RPM", std::to_string((int)std::lround(gdp->rpm)));
                        ui_draw_kv_line(renderer, gfx.ui_font, tx, ty, lh, "Deviation", fmt2(gdp->deviation) + " deg");
                        ui_draw_kv_line(renderer, gfx.ui_font, tx, ty, lh, "Pellets", std::to_string(gdp->pellets_per_shot));
                        ui_draw_kv_line(renderer, gfx.ui_font, tx, ty, lh, "Recoil", fmt2(gdp->recoil));
                        ui_draw_kv_line(renderer, gfx.ui_font, tx, ty, lh, "Control", fmt2(gdp->control));
                        ui_draw_kv_line(renderer, gfx.ui_font, tx, ty, lh, "Recoil cap", std::to_string((int)std::lround(gdp->max_recoil_spread_deg)) + " deg");
                        ui_draw_kv_line(renderer, gfx.ui_font, tx, ty, lh, "Reload/Eject", std::to_string((int)std::lround(gdp->reload_time * 1000.0f)) + "/" + std::to_string((int)std::lround(gdp->eject_time * 1000.0f)) + " ms");
                        ui_draw_kv_line(renderer, gfx.ui_font, tx, ty, lh, "Jam", std::to_string((int)std::lround(gdp->jam_chance * 100.0f)) + " %");
                        ui_draw_kv_line(renderer, gfx.ui_font, tx, ty, lh, "AR Center", fmt2(gdp->ar_pos) + " ±" + fmt2(gdp->ar_pos_variance));
                        ui_draw_kv_line(renderer, gfx.ui_font, tx, ty, lh, "AR Size", fmt2(gdp->ar_size) + " ±" + fmt2(gdp->ar_size_variance));
                        if (gim && gim->ammo_type != 0) {
                            if (auto const* ad = g_lua_mgr->find_ammo(gim->ammo_type)) {
                                int asid = (!ad->sprite.empty() && g_sprite_ids) ? g_sprite_ids->try_get(ad->sprite) : -1;
                                if (asid >= 0 && g_textures) if (SDL_Texture* tex = g_textures->get(asid)) { SDL_Rect dst{tx, ty, 36, 20}; SDL_RenderCopy(renderer, tex, nullptr, &dst); ty += 22; }
                                int apct = (int)std::lround(ad->armor_pen * 100.0f);
                                ui_draw_kv_line(renderer, gfx.ui_font, tx, ty, lh, "Ammo", ad->name);
                                if (!ad->desc.empty()) ui_draw_kv_line(renderer, gfx.ui_font, tx, ty, lh, "Desc", ad->desc);
                                ui_draw_kv_line(renderer, gfx.ui_font, tx, ty, lh, "DMG", fmt2(ad->damage_mult));
                                ui_draw_kv_line(renderer, gfx.ui_font, tx, ty, lh, "AP", std::to_string(apct) + "%");
                                ui_draw_kv_line(renderer, gfx.ui_font, tx, ty, lh, "Shield", fmt2(ad->shield_mult));
                                if (ad->range_units > 0.0f) {
                                    ui_draw_kv_line(renderer, gfx.ui_font, tx, ty, lh, "Range", std::to_string((int)std::lround(ad->range_units)));
                                    ui_draw_kv_line(renderer, gfx.ui_font, tx, ty, lh, "Falloff", std::to_string((int)std::lround(ad->falloff_start)) + "→" + std::to_string((int)std::lround(ad->falloff_end)));
                                    ui_draw_kv_line(renderer, gfx.ui_font, tx, ty, lh, "Min Mult", fmt2(ad->falloff_min_mult));
                                }
                                ui_draw_kv_line(renderer, gfx.ui_font, tx, ty, lh, "Speed", std::to_string((int)std::lround(ad->speed)));
                                ui_draw_kv_line(renderer, gfx.ui_font, tx, ty, lh, "Pierce", std::to_string(ad->pierce_count));
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
            SDL_FPoint c = world_to_screen(proj.pos.x - proj.size.x * 0.5f, proj.pos.y - proj.size.y * 0.5f);
            float scale = TILE_SIZE * gfx.play_cam.zoom;
            SDL_Rect r{(int)std::floor(c.x), (int)std::floor(c.y),
                       (int)std::ceil(proj.size.x * scale), (int)std::ceil(proj.size.y * scale)};
            bool drew = false;
            if (proj.sprite_id >= 0) {
                if (g_textures) { if (SDL_Texture* tex = g_textures->get(proj.sprite_id)) {
                    SDL_RenderCopy(renderer, tex, nullptr, &r);
                    drew = true;
                } else {
                    add_warning("Missing texture for projectile sprite");
                }
                }
            }
            if (!drew) {
                add_warning("Missing sprite for projectile");
                SDL_SetRenderDrawColor(renderer, 240, 80, 80, 255);
                SDL_RenderFillRect(renderer, &r);
            }
        }

    // draw cursor crosshair + circle + reload/jam UI
    if (state.mode == ids::MODE_PLAYING) {
        int mx = state.mouse_inputs.pos.x;
        int my = state.mouse_inputs.pos.y;
        if (state.reticle_shake > 0.01f) {
            static thread_local std::mt19937 rng{std::random_device{}()};
            std::uniform_real_distribution<float> J(-state.reticle_shake, state.reticle_shake);
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
        // Reticle circle radius from spread model
        float reticle_radius_px = 12.0f;
        if (state.player_vid) {
            const Entity* plv = state.entities.get(*state.player_vid);
            if (plv && plv->equipped_gun_vid.has_value() && g_lua_mgr) {
                const GunInstance* gi = state.guns.get(*plv->equipped_gun_vid);
                const GunDef* gd = nullptr;
                if (gi) { for (auto const& g : g_lua_mgr->guns()) if (g.type == gi->def_type) { gd = &g; break; } }
                if (gd) {
                    float acc = std::max(0.1f, plv->stats.accuracy / 100.0f);
                    float base_dev = gd->deviation / acc;
                    float move_spread = plv->move_spread_deg / acc;
                    float recoil_spread = gi->spread_recoil_deg;
                    float theta_deg = std::clamp(base_dev + move_spread + recoil_spread, MIN_SPREAD_DEG, MAX_SPREAD_DEG);
                    int ww = 0, wh = 0; SDL_GetRendererOutputSize(renderer, &ww, &wh);
                    float inv_scale = 1.0f / (TILE_SIZE * gfx.play_cam.zoom);
                    glm::vec2 ppos = plv->pos;
                    glm::vec2 mpos = {gfx.play_cam.pos.x + (static_cast<float>(state.mouse_inputs.pos.x) - (float)ww * 0.5f) * inv_scale,
                                      gfx.play_cam.pos.y + (static_cast<float>(state.mouse_inputs.pos.y) - (float)wh * 0.5f) * inv_scale};
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
            float ang = static_cast<float>(i) * (2.0f * 3.14159265358979323846f / (float)segments);
            float x = (float)mx + std::cos(ang) * (float)radius;
            float y = (float)my + std::sin(ang) * (float)radius;
            SDL_RenderDrawLine(renderer, (int)prevx, (int)prevy, (int)x, (int)y);
            prevx = x; prevy = y;
        }
        // Vertical mag + reserve bars, active reload window, text, and unjam progress
        if (state.player_vid) {
            const Entity* plv = state.entities.get(*state.player_vid);
            if (plv && plv->equipped_gun_vid.has_value() && g_lua_mgr) {
                const GunInstance* gi = state.guns.get(*plv->equipped_gun_vid);
                if (gi) {
                    const GunDef* gd = nullptr; for (auto const& g : g_lua_mgr->guns()) if (g.type == gi->def_type) { gd = &g; break; }
                    if (gd) {
                        int bar_h = 60, bar_w = 8, gap = 2; int rx = mx + 16; int ry = my - bar_h / 2;
                        if (state.reload_bar_shake > 0.01f) { static thread_local std::mt19937 rng{std::random_device{}()}; std::uniform_real_distribution<float> J(-state.reload_bar_shake, state.reload_bar_shake); rx += (int)std::lround(J(rng)); ry += (int)std::lround(J(rng)); state.reload_bar_shake *= 0.90f; } else { state.reload_bar_shake = 0.0f; }
                        float mag_ratio = (gd->mag > 0) ? (float)gi->current_mag / (float)gd->mag : 0.0f;
                        float res_ratio = (gd->ammo_max > 0) ? (float)gi->ammo_reserve / (float)gd->ammo_max : 0.0f;
                        bool reloading = (gi->reloading || gi->reload_eject_remaining > 0.0f);
                        if (!reloading) {
                            SDL_Rect bg{rx, ry, bar_w, bar_h}; SDL_SetRenderDrawColor(renderer, 40, 40, 50, 180); SDL_RenderFillRect(renderer, &bg);
                            int fill_h = (int)std::lround((double)bar_h * (double)mag_ratio);
                            SDL_Rect fg{rx, ry + (bar_h - fill_h), bar_w, fill_h}; SDL_SetRenderDrawColor(renderer, 200, 240, 255, 220); SDL_RenderFillRect(renderer, &fg);
                            SDL_Rect bg2{rx + bar_w + gap, ry, 3, bar_h}; SDL_SetRenderDrawColor(renderer, 40, 40, 50, 180); SDL_RenderFillRect(renderer, &bg2);
                            int rfill = (int)std::lround((double)bar_h * (double)res_ratio);
                            SDL_Rect rf{rx + bar_w + gap, ry + (bar_h - rfill), 3, rfill}; SDL_SetRenderDrawColor(renderer, 180, 200, 200, 220); SDL_RenderFillRect(renderer, &rf);
                        } else {
                            SDL_Rect bg{rx, ry, bar_w, bar_h}; SDL_SetRenderDrawColor(renderer, 40, 40, 50, 180); SDL_RenderFillRect(renderer, &bg);
                            if (gi->reload_total_time > 0.0f) {
                                float ws = std::clamp(gi->ar_window_start, 0.0f, 1.0f);
                                float we = std::clamp(gi->ar_window_end, 0.0f, 1.0f);
                                int wy0 = ry + (int)std::lround((double)bar_h * (1.0 - (double)we));
                                int wy1 = ry + (int)std::lround((double)bar_h * (1.0 - (double)ws));
                                SDL_Rect win{rx - 2, wy0, bar_w + 6, std::max(2, wy1 - wy0)};
                                bool lockout = (gi->ar_consumed && gi->ar_failed_attempt);
                                if (lockout) SDL_SetRenderDrawColor(renderer, 120, 120, 120, 140); else SDL_SetRenderDrawColor(renderer, 240, 220, 80, 120);
                                SDL_RenderFillRect(renderer, &win);
                                int prg_h = (int)std::lround((double)bar_h * (double)gi->reload_progress);
                                if (prg_h > 0) { SDL_Rect prog{rx - 2, ry + (bar_h - prg_h), bar_w + 6, prg_h}; if (lockout) SDL_SetRenderDrawColor(renderer, 110, 110, 120, 220); else SDL_SetRenderDrawColor(renderer, 200, 240, 255, 200); SDL_RenderFillRect(renderer, &prog); }
                            }
                            SDL_Rect bg2{rx + bar_w + gap, ry, 3, bar_h}; SDL_SetRenderDrawColor(renderer, 40, 40, 50, 180); SDL_RenderFillRect(renderer, &bg2);
                            int rfill = (int)std::lround((double)bar_h * (double)res_ratio);
                            SDL_Rect rf{rx + bar_w + gap, ry + (bar_h - rfill), 3, rfill}; SDL_SetRenderDrawColor(renderer, 180, 200, 200, 220); SDL_RenderFillRect(renderer, &rf);
                        }
                        // Text label and jam progress
                        if (gfx.ui_font) {
                            const char* txt = nullptr; SDL_Color col{250,220,80,255};
                            if (gi->jammed) { txt = "JAMMED!"; col = SDL_Color{240,80,80,255}; }
                            else if (gi->current_mag == 0) { txt = (gi->ammo_reserve > 0) ? "RELOAD" : "NO AMMO"; col = SDL_Color{250,220,80,255}; }
                            if (txt) { if (SDL_Surface* s = TTF_RenderUTF8_Blended(gfx.ui_font, txt, col)) { SDL_Texture* t = SDL_CreateTextureFromSurface(renderer, s); int tw=0,th=0; SDL_QueryTexture(t,nullptr,nullptr,&tw,&th); SDL_Rect d{rx - 4, ry - th - 4, tw, th}; SDL_RenderCopy(renderer, t, nullptr, &d); SDL_DestroyTexture(t); SDL_FreeSurface(s);} }
                        }
                        if (gi->jammed) { SDL_Rect jb{rx - 12, ry, 4, bar_h}; SDL_SetRenderDrawColor(renderer, 50, 30, 30, 200); SDL_RenderFillRect(renderer, &jb); int jh = (int)std::lround((double)bar_h * (double)gi->unjam_progress); SDL_Rect jf{rx - 12, ry + (bar_h - jh), 4, jh}; SDL_SetRenderDrawColor(renderer, 240, 60, 60, 240); SDL_RenderFillRect(renderer, &jf); }
                    }
                }
            }
        }
    }

    // Character stats panel (left slide-out)
    {
        float target = state.show_character_panel ? 1.0f : 0.0f;
        state.character_panel_slide = state.character_panel_slide + (target - state.character_panel_slide) * (float)std::clamp(6.0 * dt_sec, 0.0, 1.0);
        // Render when slightly open to avoid flicker
        if (state.character_panel_slide > 0.02f && gfx.ui_font) {
            int ww = 0, wh = 0; SDL_GetRendererOutputSize(renderer, &ww, &wh);
            int panel_w = (int)std::lround(ww * 0.28);
            int px = (int)std::lround((-panel_w + 16) * (double)(1.0f - state.character_panel_slide));
            int py = (int)std::lround(wh * 0.14);
            SDL_Rect box{px, py, panel_w, 460};
            SDL_SetRenderDrawColor(renderer, 25, 25, 30, 220); SDL_RenderFillRect(renderer, &box);
            SDL_SetRenderDrawColor(renderer, 200, 200, 220, 255); SDL_RenderDrawRect(renderer, &box);
            int tx = px + 12; int ty = py + 12; int lh = 18;
            auto draw_line = [&](const char* key, float val, const char* suffix){
                char buf[64]; std::snprintf(buf, sizeof(buf), "%s: %.2f%s", key, (double)val, suffix);
                SDL_Color col{220,220,220,255};
                if (SDL_Surface* srf = TTF_RenderUTF8_Blended(gfx.ui_font, buf, col)) { SDL_Texture* t=SDL_CreateTextureFromSurface(renderer,srf); int tw=0,th=0; SDL_QueryTexture(t,nullptr,nullptr,&tw,&th); SDL_Rect d{tx,ty,tw,th}; SDL_RenderCopy(renderer,t,nullptr,&d); SDL_DestroyTexture(t); SDL_FreeSurface(srf);} ty += lh;
            };
            const Entity* p = (state.player_vid ? state.entities.get(*state.player_vid) : nullptr);
            if (p) {
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
    }

    // Inventory list (left column) – gameplay only, hidden if character panel shown
    if (gfx.ui_font && state.mode == ids::MODE_PLAYING && !state.show_character_panel) {
        int sx = 40;
        int sy = 140;
        int slot_h = 26;
        int slot_w = 220;
        struct HoverSlot { SDL_Rect r; std::size_t index; };
        std::vector<HoverSlot> inv_hover_rects;
        for (int i = 0; i < 10; ++i) {
            bool selected = (state.inventory.selected_index == (std::size_t)i);
            SDL_Rect slot{sx, sy + i * slot_h, slot_w, slot_h - 6};
            SDL_SetRenderDrawColor(renderer, selected ? 30 : 18, selected ? 30 : 18, selected ? 40 : 22, 200);
            SDL_RenderFillRect(renderer, &slot);
            SDL_SetRenderDrawColor(renderer, 60, 60, 70, 255);
            SDL_RenderDrawRect(renderer, &slot);
            // hotkey label
            char hk[4];
            std::snprintf(hk, sizeof(hk), "%d", (i == 9) ? 0 : (i + 1));
            SDL_Color hotc{150, 150, 150, 220};
            if (SDL_Surface* hs = TTF_RenderUTF8_Blended(gfx.ui_font, hk, hotc)) {
                SDL_Texture* ht = SDL_CreateTextureFromSurface(renderer, hs);
                int tw = 0, th = 0; SDL_QueryTexture(ht, nullptr, nullptr, &tw, &th);
                SDL_Rect d{slot.x - 20, slot.y + 2, tw, th}; SDL_RenderCopy(renderer, ht, nullptr, &d);
                SDL_DestroyTexture(ht); SDL_FreeSurface(hs);
            }
            const InvEntry* ent = state.inventory.get((std::size_t)i);
            if (ent) {
                inv_hover_rects.push_back(HoverSlot{slot, (std::size_t)i});
                int icon_w = slot_h - 8, icon_h = slot_h - 8;
                int icon_x = slot.x + 6, icon_y = slot.y + 3;
                int label_x = slot.x + 8; int label_offset = 0;
                auto draw_icon = [&](int sprite_id) {
                    if (sprite_id >= 0 && g_textures) {
                        if (SDL_Texture* tex = g_textures->get(sprite_id)) {
                            SDL_Rect dst{icon_x, icon_y, icon_w, icon_h};
                            SDL_RenderCopy(renderer, tex, nullptr, &dst);
                            label_offset = icon_w + 10;
                        }
                    }
                };
                std::string label;
                if (ent->kind == INV_ITEM) {
                    if (const ItemInstance* inst = state.items.get(ent->vid)) {
                        std::string nm = "item"; uint32_t count = inst->count; int sid = -1;
                        if (g_lua_mgr) {
                            for (auto const& d : g_lua_mgr->items()) if (d.type == inst->def_type) {
                                nm = d.name; if (!d.sprite.empty() && d.sprite.find(':') != std::string::npos && g_sprite_ids) sid = g_sprite_ids->try_get(d.sprite); break; }
                        }
                        draw_icon(sid); label = nm; if (count > 1) label += std::string(" x") + std::to_string(count);
                    }
                } else if (ent->kind == INV_GUN) {
                    if (const GunInstance* gi = state.guns.get(ent->vid)) {
                        std::string nm = "gun"; int sid = -1;
                        if (g_lua_mgr) {
                            for (auto const& g : g_lua_mgr->guns()) if (g.type == gi->def_type) { nm = g.name; if (!g.sprite.empty() && g.sprite.find(':') != std::string::npos && g_sprite_ids) sid = g_sprite_ids->try_get(g.sprite); break; }
                        }
                        draw_icon(sid); label = nm;
                    }
                }
                if (!label.empty()) {
                    SDL_Color tc{230, 230, 230, 255};
                    if (SDL_Surface* ts = TTF_RenderUTF8_Blended(gfx.ui_font, label.c_str(), tc)) {
                        SDL_Texture* tt = SDL_CreateTextureFromSurface(renderer, ts);
                        int tw = 0, th = 0; SDL_QueryTexture(tt, nullptr, nullptr, &tw, &th);
                        SDL_Rect d{label_x + label_offset, slot.y + 2, tw, th}; SDL_RenderCopy(renderer, tt, nullptr, &d);
                        SDL_DestroyTexture(tt); SDL_FreeSurface(ts);
                    }
                }
            }
        }
        // Drop mode hint
        if (state.drop_mode) {
            SDL_Color hintc{230, 220, 80, 255}; const char* hint = "Drop mode: press 1–0";
            if (SDL_Surface* hs = TTF_RenderUTF8_Blended(gfx.ui_font, hint, hintc)) {
                SDL_Texture* ht = SDL_CreateTextureFromSurface(renderer, hs);
                int tw = 0, th = 0; SDL_QueryTexture(ht, nullptr, nullptr, &tw, &th);
                SDL_Rect d{sx, sy - th - 8, tw, th}; SDL_RenderCopy(renderer, ht, nullptr, &d);
                SDL_DestroyTexture(ht); SDL_FreeSurface(hs);
            }
        }
        // Hover handling + center panel
        SDL_Point mp{state.mouse_inputs.pos.x, state.mouse_inputs.pos.y};
        std::optional<std::size_t> hover_index{};
        for (auto const& hs : inv_hover_rects) {
            if (mp.x >= hs.r.x && mp.x <= (hs.r.x + hs.r.w) && mp.y >= hs.r.y && mp.y <= (hs.r.y + hs.r.h)) { hover_index = hs.index; break; }
        }
        if (hover_index.has_value()) {
            if (state.inv_hover_index == (int)*hover_index) state.inv_hover_time += (float)dt_sec; else { state.inv_hover_index = (int)*hover_index; state.inv_hover_time = 0.0f; }
        } else { state.inv_hover_index = -1; state.inv_hover_time = 0.0f; }
        const float HOVER_DELAY = 0.12f;
        // Drag-and-drop
        static bool prev_left = false; bool now_left = state.mouse_inputs.left;
        if (now_left && !prev_left) { if (hover_index.has_value()) if (state.inventory.get(*hover_index) != nullptr) { state.inv_dragging = true; state.inv_drag_src = (int)*hover_index; } }
        if (!now_left && prev_left) {
            if (state.inv_dragging) {
                if (hover_index.has_value()) {
                    std::size_t dst = *hover_index; std::size_t src = (std::size_t)std::max(0, state.inv_drag_src);
                    if (dst != src) {
                        InvEntry* src_e = state.inventory.get_mut(src); InvEntry* dst_e = state.inventory.get_mut(dst);
                        if (src_e && dst_e) { std::size_t tmp = src_e->index; src_e->index = dst_e->index; dst_e->index = tmp; }
                        else if (src_e && !dst_e) { src_e->index = dst; }
                        std::sort(state.inventory.entries.begin(), state.inventory.entries.end(), [](auto const& lhs, auto const& rhs) { return lhs.index < rhs.index; });
                    }
                }
                state.inv_dragging = false; state.inv_drag_src = -1;
            }
        }
        prev_left = now_left;
        if (hover_index.has_value() && state.inv_hover_time >= HOVER_DELAY) {
            const InvEntry* sel = state.inventory.get(*hover_index);
            if (sel) {
                int panel_w = (int)std::lround(width * 0.32);
                int px = (width - panel_w) / 2; int py = (int)std::lround(height * 0.22);
                SDL_Rect box{px, py, panel_w, 420};
                SDL_SetRenderDrawColor(renderer, 25, 25, 30, 220); SDL_RenderFillRect(renderer, &box);
                SDL_SetRenderDrawColor(renderer, 200, 200, 220, 255); SDL_RenderDrawRect(renderer, &box);
                int tx = px + 12; int ty = py + 12; int lh = 18;
                auto draw_txt = [&](const std::string& s, SDL_Color col) {
                    SDL_Surface* srf = TTF_RenderUTF8_Blended(gfx.ui_font, s.c_str(), col);
                    if (srf) { SDL_Texture* t = SDL_CreateTextureFromSurface(renderer, srf); int tw=0,th=0; SDL_QueryTexture(t,nullptr,nullptr,&tw,&th); SDL_Rect d{tx, ty, tw, th}; SDL_RenderCopy(renderer, t, nullptr, &d); SDL_DestroyTexture(t); SDL_FreeSurface(srf);} ty += lh; };
                if (sel->kind == INV_ITEM) {
                    if (const ItemInstance* inst = state.items.get(sel->vid)) {
                        std::string nm = "item"; std::string desc; uint32_t maxc = 1; bool consume = false; int sid = -1;
                        if (g_lua_mgr) { for (auto const& d : g_lua_mgr->items()) if (d.type == inst->def_type) { nm=d.name; desc=d.desc; maxc=(uint32_t)d.max_count; consume=d.consume_on_use; if(!d.sprite.empty() && d.sprite.find(':')!=std::string::npos && g_sprite_ids) sid=g_sprite_ids->try_get(d.sprite); break; } }
                        if (sid >= 0 && g_textures) if (SDL_Texture* tex = g_textures->get(sid)) { SDL_Rect dst{tx, ty, 48, 32}; SDL_RenderCopy(renderer, tex, nullptr, &dst); ty += 36; }
                        draw_txt(std::string("Item: ") + nm, SDL_Color{255,255,255,255});
                        draw_txt(std::string("Count: ") + std::to_string(inst->count) + "/" + std::to_string(maxc), SDL_Color{220,220,220,255});
                        if (!desc.empty()) draw_txt(std::string("Desc: ") + desc, SDL_Color{200,200,200,255});
                        draw_txt(std::string("Consumable: ") + (consume ? "Yes" : "No"), SDL_Color{220,220,220,255});
                    }
                } else if (sel->kind == INV_GUN) {
                    if (const GunInstance* gi = state.guns.get(sel->vid)) {
                        std::string nm = "gun"; const GunDef* gdp = nullptr;
                        if (g_lua_mgr) { for (auto const& g : g_lua_mgr->guns()) if (g.type == gi->def_type) { gdp = &g; break; } }
                        if (gdp) {
                            int gun_sid = -1; if (!gdp->sprite.empty() && g_sprite_ids) gun_sid = g_sprite_ids->try_get(gdp->sprite);
                            if (gun_sid >= 0 && g_textures) if (SDL_Texture* tex = g_textures->get(gun_sid)) { SDL_Rect dst{tx, ty, 64, 40}; SDL_RenderCopy(renderer, tex, nullptr, &dst); ty += 44; }
                            draw_txt(std::string("Gun: ") + gdp->name, SDL_Color{255,255,255,255});
                            draw_txt(std::string("Damage: ") + std::to_string((int)std::lround(gdp->damage)), SDL_Color{220,220,220,255});
                            draw_txt(std::string("RPM: ") + std::to_string((int)std::lround(gdp->rpm)), SDL_Color{220,220,220,255});
                            draw_txt(std::string("Deviation: ") + std::to_string(gdp->deviation) + " deg", SDL_Color{220,220,220,255});
                            draw_txt(std::string("Pellets: ") + std::to_string(gdp->pellets_per_shot), SDL_Color{220,220,220,255});
                            draw_txt(std::string("Recoil: ") + std::to_string(gdp->recoil), SDL_Color{220,220,220,255});
                            draw_txt(std::string("Control: ") + std::to_string(gdp->control), SDL_Color{220,220,220,255});
                            draw_txt(std::string("Recoil cap: ") + std::to_string((int)std::lround(gdp->max_recoil_spread_deg)) + " deg", SDL_Color{220,220,220,255});
                            draw_txt(std::string("Reload/Eject: ") + std::to_string((int)std::lround(gdp->reload_time * 1000.0f)) + "/" + std::to_string((int)std::lround(gdp->eject_time * 1000.0f)) + " ms", SDL_Color{220,220,220,255});
                            draw_txt(std::string("Jam: ") + std::to_string((int)std::lround(gdp->jam_chance * 100.0f)) + " %", SDL_Color{220,220,220,255});
                            if (gi->ammo_type != 0) {
                                if (auto const* ad = g_lua_mgr->find_ammo(gi->ammo_type)) {
                                    int asid = (!ad->sprite.empty() && g_sprite_ids) ? g_sprite_ids->try_get(ad->sprite) : -1;
                                    if (asid >= 0 && g_textures) if (SDL_Texture* tex = g_textures->get(asid)) { SDL_Rect dst{tx, ty, 36, 20}; SDL_RenderCopy(renderer, tex, nullptr, &dst); ty += 22; }
                                    draw_txt(std::string("Ammo: ") + ad->name, SDL_Color{255,255,255,255});
                                    if (!ad->desc.empty()) draw_txt(std::string("Desc: ") + ad->desc, SDL_Color{200,200,200,255});
                                    int apct = (int)std::lround(ad->armor_pen * 100.0f);
                                    draw_txt(std::string("Ammo Stats: DMG x") + std::to_string(ad->damage_mult) + std::string(", AP ") + std::to_string(apct) + "%" + std::string(", Shield x") + std::to_string(ad->shield_mult), SDL_Color{220,220,220,255});
                                    if (ad->range_units > 0.0f) {
                                        draw_txt(std::string("Range: ") + std::to_string((int)std::lround(ad->range_units)) + std::string(" (falloff ") + std::to_string((int)std::lround(ad->falloff_start)) + std::string("→") + std::to_string((int)std::lround(ad->falloff_end)) + std::string(" @ x") + std::to_string(ad->falloff_min_mult) + ")", SDL_Color{220,220,220,255});
                                    }
                                    draw_txt(std::string("Speed: ") + std::to_string((int)std::lround(ad->speed)) + std::string(", Pierce: ") + std::to_string(ad->pierce_count), SDL_Color{220,220,220,255});
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    // Right-side equipped gun info panel (toggle with V)
    if (gfx.ui_font && state.mode == ids::MODE_PLAYING && state.player_vid && g_lua_mgr && state.show_gun_panel) {
        const Entity* ply = state.entities.get(*state.player_vid);
        if (ply && ply->equipped_gun_vid.has_value()) {
            const GunDef* gd = nullptr; const GunInstance* gi_inst = state.guns.get(*ply->equipped_gun_vid);
            if (gi_inst) { for (auto const& g : g_lua_mgr->guns()) if (g.type == gi_inst->def_type) { gd = &g; break; } }
            if (gd) {
                int panel_w = (int)std::lround(width * 0.26);
                int px = width - panel_w - 30; int py = (int)std::lround(height * 0.18);
                SDL_Rect box{px, py, panel_w, 460};
                SDL_SetRenderDrawColor(renderer, 25, 25, 30, 220); SDL_RenderFillRect(renderer, &box);
                SDL_SetRenderDrawColor(renderer, 200, 200, 220, 255); SDL_RenderDrawRect(renderer, &box);
                int tx = px + 12; int ty = py + 12; int lh = 18;
                auto draw_txt = [&](const std::string& s, SDL_Color col){ SDL_Surface* srf = TTF_RenderUTF8_Blended(gfx.ui_font, s.c_str(), col); if (srf){ SDL_Texture* t=SDL_CreateTextureFromSurface(renderer,srf); int tw=0,th=0; SDL_QueryTexture(t,nullptr,nullptr,&tw,&th); SDL_Rect d{tx,ty,tw,th}; SDL_RenderCopy(renderer,t,nullptr,&d); SDL_DestroyTexture(t); SDL_FreeSurface(srf);} ty += lh; };
                // Icon
                if (!gd->sprite.empty() && g_sprite_ids) { int sid = g_sprite_ids->try_get(gd->sprite); if (sid >= 0 && g_textures) if (SDL_Texture* tex = g_textures->get(sid)) { SDL_Rect dst{tx, ty, 64, 40}; SDL_RenderCopy(renderer, tex, nullptr, &dst); ty += 44; } }
                // Key stats
                ui_draw_kv_line(renderer, gfx.ui_font, tx, ty, lh, "Gun", gd->name);
                ui_draw_kv_line(renderer, gfx.ui_font, tx, ty, lh, "Damage", std::to_string((int)std::lround(gd->damage)));
                ui_draw_kv_line(renderer, gfx.ui_font, tx, ty, lh, "RPM", std::to_string((int)std::lround(gd->rpm)));
                ui_draw_kv_line(renderer, gfx.ui_font, tx, ty, lh, "Deviation", fmt2(gd->deviation) + " deg");
                ui_draw_kv_line(renderer, gfx.ui_font, tx, ty, lh, "Pellets", std::to_string(gd->pellets_per_shot));
                ui_draw_kv_line(renderer, gfx.ui_font, tx, ty, lh, "Recoil cap", std::to_string((int)std::lround(gd->max_recoil_spread_deg)) + " deg");
                ui_draw_kv_line(renderer, gfx.ui_font, tx, ty, lh, "Reload", std::to_string((int)std::lround(gd->reload_time * 1000.0f)) + " ms");
                ui_draw_kv_line(renderer, gfx.ui_font, tx, ty, lh, "Eject", std::to_string((int)std::lround(gd->eject_time * 1000.0f)) + " ms");
                ui_draw_kv_line(renderer, gfx.ui_font, tx, ty, lh, "Jam", std::to_string((int)std::lround(gd->jam_chance * 100.0f)) + " %");
                ui_draw_kv_line(renderer, gfx.ui_font, tx, ty, lh, "AR Center", fmt2(gd->ar_pos) + " ±" + fmt2(gd->ar_pos_variance));
                ui_draw_kv_line(renderer, gfx.ui_font, tx, ty, lh, "AR Size", fmt2(gd->ar_size) + " ±" + fmt2(gd->ar_size_variance));
                if (gd->rpm > 0.0f || gd->shot_interval > 0.0f) { float dt = gd->shot_interval > 0.0f ? gd->shot_interval : (60.0f / std::max(1.0f, gd->rpm)); int ms = (int)std::lround(dt * 1000.0f); draw_txt(std::string("Shot Time: ") + std::to_string(ms) + " ms", SDL_Color{220,220,220,255}); }
                if (gd->fire_mode == std::string("burst") && (gd->burst_rpm > 0.0f || gd->burst_interval > 0.0f)) { draw_txt(std::string("Burst RPM: ") + std::to_string((int)std::lround(gd->burst_rpm)), SDL_Color{220,220,220,255}); float bdt = gd->burst_interval > 0.0f ? gd->burst_interval : (gd->burst_rpm > 0.0f ? 60.0f / gd->burst_rpm : 0.0f); if (bdt > 0.0f) { int bms = (int)std::lround(bdt * 1000.0f); draw_txt(std::string("Burst Time: ") + std::to_string(bms) + " ms", SDL_Color{220,220,220,255}); } }
                // Fire mode label
                { std::string fm_label = "Auto"; if (gd->fire_mode == "single") fm_label = "Semi"; else if (gd->fire_mode == "burst") fm_label = "Burst"; draw_txt(std::string("Mode: ") + fm_label, SDL_Color{220,220,220,255}); }
                // Current mag/reserve
                if (gi_inst) {
                    ui_draw_kv_line(renderer, gfx.ui_font, tx, ty, lh, "Mag", std::to_string(gi_inst->current_mag));
                    ui_draw_kv_line(renderer, gfx.ui_font, tx, ty, lh, "Reserve", std::to_string(gi_inst->ammo_reserve));
                    if (gi_inst->ammo_type != 0) {
                        if (auto const* ad = g_lua_mgr->find_ammo(gi_inst->ammo_type)) {
                            int asid = (!ad->sprite.empty() && g_sprite_ids) ? g_sprite_ids->try_get(ad->sprite) : -1;
                            if (asid >= 0 && g_textures) if (SDL_Texture* tex = g_textures->get(asid)) { SDL_Rect dst{tx, ty, 36, 20}; SDL_RenderCopy(renderer, tex, nullptr, &dst); ty += 22; }
                            int apct = (int)std::lround(ad->armor_pen * 100.0f);
                            ui_draw_kv_line(renderer, gfx.ui_font, tx, ty, lh, "Ammo", ad->name);
                            if (!ad->desc.empty()) ui_draw_kv_line(renderer, gfx.ui_font, tx, ty, lh, "Desc", ad->desc);
                            ui_draw_kv_line(renderer, gfx.ui_font, tx, ty, lh, "DMG", fmt2(ad->damage_mult));
                            ui_draw_kv_line(renderer, gfx.ui_font, tx, ty, lh, "AP", std::to_string(apct) + "%");
                            ui_draw_kv_line(renderer, gfx.ui_font, tx, ty, lh, "Shield", fmt2(ad->shield_mult));
                            if (ad->range_units > 0.0f) {
                                ui_draw_kv_line(renderer, gfx.ui_font, tx, ty, lh, "Range", std::to_string((int)std::lround(ad->range_units)));
                                ui_draw_kv_line(renderer, gfx.ui_font, tx, ty, lh, "Falloff", std::to_string((int)std::lround(ad->falloff_start)) + "→" + std::to_string((int)std::lround(ad->falloff_end)));
                                ui_draw_kv_line(renderer, gfx.ui_font, tx, ty, lh, "Min Mult", fmt2(ad->falloff_min_mult));
                            }
                            ui_draw_kv_line(renderer, gfx.ui_font, tx, ty, lh, "Speed", std::to_string((int)std::lround(ad->speed)));
                            ui_draw_kv_line(renderer, gfx.ui_font, tx, ty, lh, "Pierce", std::to_string(ad->pierce_count));
                        }
                    }
                }
            }
        }
    }

    // Alerts and warnings (screen-space)
    // Bottom player condition bars (shield, plates, health, dash)
    if (gfx.ui_font && state.mode == ids::MODE_PLAYING && state.player_vid) {
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
                std::uniform_real_distribution<float> J(-state.hp_bar_shake, state.hp_bar_shake);
                gx += (int)std::lround(J(rng));
                gy += (int)std::lround(J(rng));
                state.hp_bar_shake *= 0.90f;
            } else {
                state.hp_bar_shake = 0.0f;
            }
            auto draw_num = [&](const std::string& s, int x, int y, SDL_Color col) {
                SDL_Surface* srf = TTF_RenderUTF8_Blended(gfx.ui_font, s.c_str(), col);
                if (!srf) return SDL_Point{x, y};
                SDL_Texture* t = SDL_CreateTextureFromSurface(renderer, srf);
                int tw = 0, th = 0; SDL_QueryTexture(t, nullptr, nullptr, &tw, &th);
                SDL_Rect d{x, y + (bar_h - th) / 2, tw, th}; SDL_RenderCopy(renderer, t, nullptr, &d);
                SDL_DestroyTexture(t); SDL_FreeSurface(srf);
                return SDL_Point{d.x + d.w, d.y + d.h};
            };
            auto draw_bar = [&](int x, int y, int w, int h, float ratio, SDL_Color fill) {
                SDL_Rect bg{x, y, w, h}; SDL_SetRenderDrawColor(renderer, 20, 20, 24, 220); SDL_RenderFillRect(renderer, &bg);
                SDL_SetRenderDrawColor(renderer, 60, 60, 70, 255); SDL_RenderDrawRect(renderer, &bg);
                double rd = (double)std::clamp(ratio, 0.0f, 1.0f);
                int fw = (int)std::lround((double)w * rd);
                if (fw > 0) { SDL_Rect fr{x, y, fw, h}; SDL_SetRenderDrawColor(renderer, fill.r, fill.g, fill.b, fill.a); SDL_RenderFillRect(renderer, &fr); }
            };
            SDL_Color white{240, 240, 240, 255};
            // Shield
            if (p->stats.shield_max > 0.0f) {
                float sratio = (p->stats.shield_max > 0.0f) ? (p->shield / p->stats.shield_max) : 0.0f;
                draw_bar(gx, gy, group_w, bar_h, sratio, SDL_Color{120, 200, 240, 220});
                draw_num(std::to_string((int)std::lround(p->shield)), gx - 46, gy, white);
                draw_num(std::to_string((int)std::lround(p->stats.shield_max)), gx + group_w + 6, gy, white);
            }
            // Plates
            int gy2 = gy + bar_h + gap_y;
            draw_bar(gx, gy2, group_w, bar_h, 0.0f, SDL_Color{0, 0, 0, 0});
            {
                int to_show = std::min(20, p->stats.plates);
                int slw = 6, gap = 2;
                int start_x = gx;
                for (int i = 0; i < to_show; ++i) {
                    SDL_Rect prr{start_x + i * (slw + gap), gy2 + 2, slw, bar_h - 4};
                    SDL_SetRenderDrawColor(renderer, 80, 80, 80, 255); SDL_RenderFillRect(renderer, &prr);
                    SDL_SetRenderDrawColor(renderer, 140, 140, 140, 255); SDL_RenderDrawRect(renderer, &prr);
                }
                draw_num(std::to_string(p->stats.plates), gx - 46, gy2, white);
            }
            // Health
            int gy3 = gy2 + bar_h + gap_y;
            {
                float hratio = (p->max_hp > 0) ? ((float)p->health / (float)p->max_hp) : 0.0f;
                draw_bar(gx, gy3, group_w, bar_h, hratio, SDL_Color{220, 60, 60, 230});
                draw_num(std::to_string((int)p->health), gx - 46, gy3, white);
                draw_num(std::to_string((int)p->max_hp), gx + group_w + 6, gy3, white);
            }
            // Dash
            int gy4 = gy3 + bar_h + gap_y; draw_bar(gx, gy4, group_w, bar_h, 0.0f, SDL_Color{0, 0, 0, 0});
            if (state.dash_max > 0) {
                int segs = state.dash_max; int slw = 12; int sgap = 2; int start_x = gx;
                for (int i = 0; i < segs; ++i) {
                    int sx_dash = start_x + i * (slw + sgap);
                    SDL_Rect seg{sx_dash, gy4 + 2, slw, bar_h - 4};
                    if (i < state.dash_stocks) SDL_SetRenderDrawColor(renderer, 80, 200, 120, 220); else SDL_SetRenderDrawColor(renderer, 40, 60, 70, 200);
                    SDL_RenderFillRect(renderer, &seg);
                    SDL_SetRenderDrawColor(renderer, 20, 30, 40, 255); SDL_RenderDrawRect(renderer, &seg);
                }
                if (state.dash_stocks < state.dash_max) {
                    double pratio = std::clamp((double)(state.dash_refill_timer / DASH_COOLDOWN_SECONDS), 0.0, 1.0);
                    int pw = (int)std::lround((double)group_w * pratio);
                    int psl = std::max(2, bar_h / 4);
                    SDL_Rect pbar{gx, gy4 - psl, pw, psl}; SDL_SetRenderDrawColor(renderer, 90, 200, 160, 200); SDL_RenderFillRect(renderer, &pbar);
                }
            }
        }
    }

    // Exit countdown overlay (top) when standing on exit
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
        int fgw = (int)std::lround((double)bar_w * (double)(1.0f - ratio));
        SDL_Rect fg{bar_x, bar_y, fgw, bar_h};
        SDL_SetRenderDrawColor(renderer, 240, 220, 80, 220);
        SDL_RenderFillRect(renderer, &fg);
        SDL_SetRenderDrawColor(renderer, 10, 10, 10, 255);
        SDL_RenderDrawRect(renderer, &bg);
        if (gfx.ui_font) {
            const char* label = "Exiting to next area";
            SDL_Color lc{240, 220, 80, 255};
            if (SDL_Surface* lsurf = TTF_RenderUTF8_Blended(gfx.ui_font, label, lc)) {
                SDL_Texture* ltex = SDL_CreateTextureFromSurface(renderer, lsurf);
                if (ltex) { int lw=0,lh=0; SDL_QueryTexture(ltex,nullptr,nullptr,&lw,&lh); SDL_Rect ldst{bar_x, bar_y - lh - 6, lw, lh}; SDL_RenderCopy(renderer, ltex, nullptr, &ldst); SDL_DestroyTexture(ltex);} SDL_FreeSurface(lsurf);
            }
            char txt[16]; float secs = std::max(0.0f, state.exit_countdown); std::snprintf(txt, sizeof(txt), "%.1f", (double)secs);
            SDL_Color color{255, 255, 255, 255};
            if (SDL_Surface* surf = TTF_RenderUTF8_Blended(gfx.ui_font, txt, color)) { SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surf); if (tex) { int tw=0,th=0; SDL_QueryTexture(tex,nullptr,nullptr,&tw,&th); SDL_Rect dst{bar_x + bar_w / 2 - tw / 2, bar_y - th - 4, tw, th}; SDL_RenderCopy(renderer, tex, nullptr, &dst); SDL_DestroyTexture(tex);} SDL_FreeSurface(surf); }
        }
    }

    // Alerts and warnings (screen-space)
    if (gfx.ui_font) {
        int ww = 0, wh = 0;
        SDL_GetRendererOutputSize(renderer, &ww, &wh);
        int ax = 12, ay = 12, lh = 18;
        // Alerts in white-ish
        for (const auto& al : state.alerts) {
            std::string msg = al.text;
            SDL_Color col{230, 230, 240, 255};
            SDL_Surface* s = TTF_RenderUTF8_Blended(gfx.ui_font, msg.c_str(), col);
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
        // Warnings in red
        for (const auto& msg : frame_warnings) {
            SDL_Color col2{220, 60, 60, 255};
            SDL_Surface* s = TTF_RenderUTF8_Blended(gfx.ui_font, msg.c_str(), col2);
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

    // Score review overlay (animated metrics)
    if (state.mode == ids::MODE_SCORE_REVIEW) {
        SDL_Rect full{0, 0, width, height};
        SDL_SetRenderDrawColor(renderer, 18, 18, 22, 255);
        SDL_RenderFillRect(renderer, &full);
        if (gfx.ui_font) {
            SDL_Color titlec{240, 220, 80, 255};
            if (SDL_Surface* ts = TTF_RenderUTF8_Blended(gfx.ui_font, "Stage Clear", titlec)) {
                SDL_Texture* tt = SDL_CreateTextureFromSurface(renderer, ts);
                int tw=0, th=0; SDL_QueryTexture(tt, nullptr, nullptr, &tw, &th);
                SDL_Rect td{40, 40, tw, th}; SDL_RenderCopy(renderer, tt, nullptr, &td);
                SDL_DestroyTexture(tt); SDL_FreeSurface(ts);
            }
            if (state.score_ready_timer <= 0.0f) {
                SDL_Color pc{200, 200, 210, 255};
                if (SDL_Surface* ps = TTF_RenderUTF8_Blended(gfx.ui_font, "Press SPACE or CLICK to continue", pc)) {
                    SDL_Texture* ptex = SDL_CreateTextureFromSurface(renderer, ps);
                    int prompt_w=0, prompt_h=0; SDL_QueryTexture(ptex, nullptr, nullptr, &prompt_w, &prompt_h);
                    SDL_Rect pd{width/2 - prompt_w/2, height - prompt_h - 20, prompt_w, prompt_h};
                    SDL_RenderCopy(renderer, ptex, nullptr, &pd);
                    SDL_DestroyTexture(ptex); SDL_FreeSurface(ps);
                }
            }
        }
        if (state.score_ready_timer <= 0.0f) {
            state.review_next_stat_timer -= (float)dt_sec;
            if (state.review_next_stat_timer <= 0.0f && state.review_revealed < state.review_stats.size()) {
                state.review_next_stat_timer = 0.2f;
                state.review_revealed += 1;
                if (g_audio) g_audio->sounds.play("base:small_shoot");
            }
            state.review_number_tick_timer += (float)dt_sec;
            while (state.review_number_tick_timer >= 0.05f) {
                state.review_number_tick_timer -= 0.05f;
                for (std::size_t i = 0; i < state.review_revealed; ++i) {
                    auto& rs = state.review_stats[i];
                    if (rs.header || rs.done) continue;
                    double step = std::max(1.0, std::floor(rs.target / 20.0));
                    if (rs.target < 20.0) step = std::max(0.1, rs.target / 20.0);
                    rs.value = std::min(rs.target, rs.value + step);
                    if (g_audio) g_audio->sounds.play("base:small_shoot");
                    if (rs.value >= rs.target) rs.done = true;
                }
            }
        }
        if (gfx.ui_font) {
            int tx = 40; int ty = 80;
            auto draw_line = [&](const std::string& s, SDL_Color col){ SDL_Surface* srf = TTF_RenderUTF8_Blended(gfx.ui_font, s.c_str(), col); if (!srf) return; SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, srf); int tw=0, th=0; SDL_QueryTexture(tex, nullptr, nullptr, &tw, &th); SDL_Rect d{tx, ty, tw, th}; SDL_RenderCopy(renderer, tex, nullptr, &d); SDL_DestroyTexture(tex); SDL_FreeSurface(srf); ty += 18; };
            SDL_Color mc{210, 210, 220, 255}; char buf[128];
            for (std::size_t i = 0; i < state.review_revealed && i < state.review_stats.size(); ++i) {
                auto const& rs = state.review_stats[i];
                if (rs.header) {
                    draw_line(rs.label, SDL_Color{240, 220, 80, 255});
                } else {
                    if (std::fabs(rs.target - std::round(rs.target)) < 0.001)
                        std::snprintf(buf, sizeof(buf), "%s: %d", rs.label.c_str(), (int)std::lround(rs.value));
                    else
                        std::snprintf(buf, sizeof(buf), "%s: %.1f", rs.label.c_str(), rs.value);
                    draw_line(buf, mc);
                }
            }
        }
        if (state.score_ready_timer > 0.0f) {
            float ratio = state.score_ready_timer / SCORE_REVIEW_INPUT_DELAY;
            ratio = std::clamp(ratio, 0.0f, 1.0f);
            int wbw = (int)std::lround((double)(width - 80) * (double)ratio);
            SDL_Rect waitbar{40, height - 80, wbw, 8};
            SDL_SetRenderDrawColor(renderer, 240, 220, 80, 220);
            SDL_RenderFillRect(renderer, &waitbar);
        }
    }

    // Next-stage info page
    if (state.mode == ids::MODE_NEXT_STAGE) {
        SDL_Rect full{0, 0, width, height}; SDL_SetRenderDrawColor(renderer, 18, 18, 22, 255); SDL_RenderFillRect(renderer, &full);
        int box_w = width - 200; int box_h = 140; int box_x = (width - box_w) / 2; int box_y = 40;
        SDL_Rect box{box_x, box_y, box_w, box_h}; SDL_SetRenderDrawColor(renderer, 30, 30, 40, 220); SDL_RenderFillRect(renderer, &box);
        SDL_SetRenderDrawColor(renderer, 200, 200, 220, 255); SDL_RenderDrawRect(renderer, &box);
        SDL_SetRenderDrawColor(renderer, 240, 220, 80, 255); SDL_RenderDrawLine(renderer, box_x + 20, box_y + 20, box_x + box_w - 20, box_y + 20);
        if (gfx.ui_font) {
            SDL_Color titlec{240, 220, 80, 255}; if (SDL_Surface* ts = TTF_RenderUTF8_Blended(gfx.ui_font, "Next Area", titlec)) { SDL_Texture* tt = SDL_CreateTextureFromSurface(renderer, ts); int tw=0, th=0; SDL_QueryTexture(tt,nullptr,nullptr,&tw,&th); SDL_Rect td{box_x + 24, box_y + 16, tw, th}; SDL_RenderCopy(renderer, tt, nullptr, &td); SDL_DestroyTexture(tt); SDL_FreeSurface(ts); }
            if (state.score_ready_timer <= 0.0f) { SDL_Color pc{200, 200, 210, 255}; if (SDL_Surface* ps = TTF_RenderUTF8_Blended(gfx.ui_font, "Press SPACE or CLICK to continue", pc)) { SDL_Texture* ptex = SDL_CreateTextureFromSurface(renderer, ps); int prompt_w=0,prompt_h=0; SDL_QueryTexture(ptex,nullptr,nullptr,&prompt_w,&prompt_h); SDL_Rect pd{width/2 - prompt_w/2, height - prompt_h - 20, prompt_w, prompt_h}; SDL_RenderCopy(renderer, ptex, nullptr, &pd); SDL_DestroyTexture(ptex); SDL_FreeSurface(ps);} }
        }
    }

    SDL_RenderPresent(renderer);
}
