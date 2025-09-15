// Manual and auto pickups, hotbar actions, drop mode, and simple ground separation.
#include "globals.hpp"
#include "state.hpp"
#include "items.hpp"
#include "guns.hpp"
#include "audio.hpp"
#include "room.hpp"
#include "luamgr.hpp"
#include "settings.hpp"

#include <glm/glm.hpp>

void auto_pickup_powerups() {
    if (!ss) return;
    if (ss->mode != ids::MODE_PLAYING || !ss->player_vid) return;
    const Entity* p = ss->entities.get(*ss->player_vid);
    if (!p) return;
    glm::vec2 ph = p->half_size();
    float pl = p->pos.x - ph.x, pr = p->pos.x + ph.x;
    float pt = p->pos.y - ph.y, pb = p->pos.y + ph.y;
    for (auto& pu : ss->pickups.data()) if (pu.active) {
        float gl = pu.pos.x - 0.125f, gr = pu.pos.x + 0.125f;
        float gt = pu.pos.y - 0.125f, gb = pu.pos.y + 0.125f;
        bool overlap = !(pr <= gl || pl >= gr || pb <= gt || pt >= gb);
        if (overlap) {
            ss->alerts.push_back({std::string("Picked up ") + pu.name, 0.0f, 2.0f, false});
            pu.active = false;
            if (ss->player_vid) if (auto* pm = ss->metrics_for(*ss->player_vid)) pm->powerups_picked += 1;
        }
    }
}

void handle_manual_pickups() {
    if (ss->mode != ids::MODE_PLAYING || !ss->player_vid) return;
    const Entity* p = ss->entities.get(*ss->player_vid); if (!p) return;
    glm::vec2 ph = p->half_size();
    float pl = p->pos.x - ph.x, pr = p->pos.x + ph.x;
    float pt = p->pos.y - ph.y, pb = p->pos.y + ph.y;
    static bool prev_pick = false;
    bool now_pick = ss->playing_inputs.pick_up;
    if (now_pick && !prev_pick && ss->pickup_lockout == 0.0f) {
        bool did_pick = false;
        enum struct PickKind { None, Gun, Item };
        PickKind best_kind = PickKind::None; std::size_t best_index = (std::size_t)-1; float best_area = 0.0f;
        auto overlap_area = [&](float al, float at, float ar, float ab, float bl, float bt, float br, float bb) -> float {
            float xl = std::max(al, bl), xr = std::min(ar, br);
            float yt = std::max(at, bt), yb = std::min(ab, bb);
            float w = xr - xl, h = yb - yt; if (w <= 0.0f || h <= 0.0f) return 0.0f; return w * h;
        };
        for (std::size_t i = 0; i < ss->ground_guns.data().size(); ++i) {
            auto const& ggun = ss->ground_guns.data()[i]; if (!ggun.active) continue;
            glm::vec2 gh = ggun.size * 0.5f; float gl = ggun.pos.x - gh.x, gr = ggun.pos.x + gh.x; float gt = ggun.pos.y - gh.y, gb = ggun.pos.y + gh.y;
            float area = overlap_area(pl, pt, pr, pb, gl, gt, gr, gb);
            if (area > best_area) { best_area = area; best_kind = PickKind::Gun; best_index = i; }
        }
        for (std::size_t i = 0; i < ss->ground_items.data().size(); ++i) {
            auto const& gi = ss->ground_items.data()[i]; if (!gi.active) continue;
            glm::vec2 gh = gi.size * 0.5f; float gl = gi.pos.x - gh.x, gr = gi.pos.x + gh.x; float gt = gi.pos.y - gh.y, gb = gi.pos.y + gh.y;
            float area = overlap_area(pl, pt, pr, pb, gl, gt, gr, gb);
            if (area > best_area) { best_area = area; best_kind = PickKind::Item; best_index = i; }
        }
        if (best_kind == PickKind::Gun) {
            auto& ggun = ss->ground_guns.data()[best_index];
            bool ok = false; if (auto* inv = (ss->player_vid ? ss->inv_for(*ss->player_vid) : nullptr)) ok = inv->insert_existing(INV_GUN, ggun.gun_vid);
            std::string nm = "gun";
            if (luam) if (const GunInstance* gi = ss->guns.get(ggun.gun_vid)) for (auto const& g : luam->guns()) if (g.type == gi->def_type) { nm = g.name; break; }
            if (ok) {
                ggun.active = false; did_pick = true; ss->alerts.push_back({std::string("Picked up ") + nm, 0.0f, 2.0f, false});
                if (ss->player_vid) if (auto* pm = ss->metrics_for(*ss->player_vid)) pm->guns_picked += 1;
                if (const GunInstance* ggi = ss->guns.get(ggun.gun_vid)) {
                    const GunDef* gd = nullptr; if (luam) for (auto const& g : luam->guns()) if (g.type == ggi->def_type) { gd = &g; break; }
                    if (luam && ss->player_vid) if (auto* plent = ss->entities.get_mut(*ss->player_vid)) luam->call_gun_on_pickup(ggi->def_type, *plent);
                    if (gd) play_sound(gd->sound_pickup.empty() ? "base:drop" : gd->sound_pickup); else play_sound("base:drop");
                }
            } else {
                ss->alerts.push_back({"Inventory full", 0.0f, 1.5f, false});
            }
        } else if (best_kind == PickKind::Item) {
            auto& gi = ss->ground_items.data()[best_index];
            std::string nm = "item"; int maxc = 1; const ItemInstance* pick = ss->items.get(gi.item_vid);
            if (luam && pick) { for (auto const& d : luam->items()) if (d.type == pick->def_type) { nm = d.name; maxc = d.max_count; break; } }
            bool fully_merged = false;
            if (pick) {
                if (auto* inv = (ss->player_vid ? ss->inv_for(*ss->player_vid) : nullptr))
                for (auto& e : inv->entries) {
                    if (e.kind != INV_ITEM) continue;
                    ItemInstance* tgt = ss->items.get(e.vid);
                    if (!tgt) continue;
                    if (tgt->def_type != pick->def_type) continue;
                    if (tgt->modifiers_hash != pick->modifiers_hash) continue;
                    if (tgt->use_cooldown_countdown > 0.0f || pick->use_cooldown_countdown > 0.0f) continue;
                    if (tgt->count >= (uint32_t)maxc) continue;
                    uint32_t space = (uint32_t)maxc - tgt->count;
                    uint32_t xfer = std::min(space, pick->count);
                    tgt->count += xfer;
                    if (auto* pmut = ss->items.get(gi.item_vid)) pmut->count -= xfer;
                    if (auto* after = ss->items.get(gi.item_vid)) {
                        if (after->count == 0) { ss->items.free(gi.item_vid); gi.active = false; fully_merged = true; }
                    }
                    if (xfer > 0) break;
                }
            }
            if (!fully_merged) {
                bool ok = false; if (auto* inv = (ss->player_vid ? ss->inv_for(*ss->player_vid) : nullptr)) ok = inv->insert_existing(INV_ITEM, gi.item_vid);
                if (ok) {
                    gi.active = false; did_pick = true; ss->alerts.push_back({std::string("Picked up ") + nm, 0.0f, 2.0f, false});
                    if (luam && pick && ss->player_vid) if (auto* plent = ss->entities.get_mut(*ss->player_vid)) luam->call_item_on_pickup(pick->def_type, *plent);
                    if (luam && pick) { const ItemDef* idf = nullptr; for (auto const& d : luam->items()) if (d.type == pick->def_type) { idf = &d; break; }
                        if (idf) play_sound(idf->sound_pickup.empty() ? "base:drop" : idf->sound_pickup); else play_sound("base:drop"); }
                    if (ss->player_vid) if (auto* pm = ss->metrics_for(*ss->player_vid)) pm->items_picked += 1;
                } else {
                    ss->alerts.push_back({std::string("Inventory full"), 0.0f, 1.5f, false});
                }
            }
        }
        if (did_pick) ss->pickup_lockout = PICKUP_DEBOUNCE_SECONDS;
    }
    prev_pick = now_pick;
}

void separate_ground_items() {
    for (auto& giA : ss->ground_items.data()) if (giA.active) {
        for (auto& giB : ss->ground_items.data()) if (&giA != &giB && giB.active) {
            glm::vec2 ah = giA.size * 0.5f, bh = giB.size * 0.5f;
            bool overlap = !((giA.pos.x + ah.x) <= (giB.pos.x - bh.x) || (giA.pos.x - ah.x) >= (giB.pos.x + bh.x) || (giA.pos.y + ah.y) <= (giB.pos.y - bh.y) || (giA.pos.y - ah.y) >= (giB.pos.y + bh.y));
            if (overlap) {
                glm::vec2 d = giA.pos - giB.pos; if (d.x == 0 && d.y == 0) d = {0.01f, 0.0f};
                float len = std::sqrt(d.x * d.x + d.y * d.y); if (len < 1e-3f) len = 1.0f; d /= len;
                giA.pos += d * 0.01f; giB.pos -= d * 0.01f;
            }
        }
    }
    for (auto& ga : ss->ground_guns.data()) if (ga.active) {
        for (auto& gb : ss->ground_guns.data()) if (&ga != &gb && gb.active) {
            glm::vec2 ah = ga.size * 0.5f, bh = gb.size * 0.5f;
            bool overlap = !((ga.pos.x + ah.x) <= (gb.pos.x - bh.x) || (ga.pos.x - ah.x) >= (gb.pos.x + bh.x) || (ga.pos.y + ah.y) <= (gb.pos.y - bh.y) || (ga.pos.y - ah.y) >= (gb.pos.y + bh.y));
            if (overlap) {
                glm::vec2 d = ga.pos - gb.pos; if (d.x == 0 && d.y == 0) d = {0.01f, 0.0f};
                float len = std::sqrt(d.x * d.x + d.y * d.y); if (len < 1e-3f) len = 1.0f; d /= len;
                ga.pos += d * 0.01f; gb.pos -= d * 0.01f;
            }
        }
    }
    for (auto& gi : ss->ground_items.data()) if (gi.active) {
        glm::vec2 ih = gi.size * 0.5f;
        for (auto& ggun : ss->ground_guns.data()) if (ggun.active) {
            glm::vec2 gh = ggun.size * 0.5f;
            bool overlap = !((gi.pos.x + ih.x) <= (ggun.pos.x - gh.x) || (gi.pos.x - ih.x) >= (ggun.pos.x + gh.x) || (gi.pos.y + ih.y) <= (ggun.pos.y - gh.y) || (gi.pos.y - ih.y) >= (ggun.pos.y + gh.y));
            if (overlap) {
                glm::vec2 d = gi.pos - ggun.pos; if (d.x == 0 && d.y == 0) d = {0.01f, 0.0f};
                float len = std::sqrt(d.x * d.x + d.y * d.y); if (len < 1e-3f) len = 1.0f; d /= len;
                gi.pos += d * 0.01f; ggun.pos -= d * 0.01f;
            }
        }
    }
    for (auto& c : ss->crates.data()) if (c.active) {
        glm::vec2 ch = c.size * 0.5f;
        for (auto& gi : ss->ground_items.data()) if (gi.active) {
            glm::vec2 ih = gi.size * 0.5f;
            bool overlap = !((gi.pos.x + ih.x) <= (c.pos.x - ch.x) || (gi.pos.x - ih.x) >= (c.pos.x + ch.x) || (gi.pos.y + ih.y) <= (c.pos.y - ch.y) || (gi.pos.y - ih.y) >= (c.pos.y + ch.y));
            if (overlap) {
                glm::vec2 d = gi.pos - c.pos; if (d.x == 0 && d.y == 0) d = {0.01f, 0.0f};
                float len = std::sqrt(d.x * d.x + d.y * d.y); if (len < 1e-3f) len = 1.0f; d /= len;
                gi.pos += d * 0.012f;
            }
        }
        for (auto& ggun : ss->ground_guns.data()) if (ggun.active) {
            glm::vec2 gh = ggun.size * 0.5f;
            bool overlap = !((ggun.pos.x + gh.x) <= (c.pos.x - ch.x) || (ggun.pos.x - gh.x) >= (c.pos.x + ch.x) || (ggun.pos.y + gh.y) <= (c.pos.y - ch.y) || (ggun.pos.y - gh.y) >= (c.pos.y + ch.y));
            if (overlap) {
                glm::vec2 d = ggun.pos - c.pos; if (d.x == 0 && d.y == 0) d = {0.01f, 0.0f};
                float len = std::sqrt(d.x * d.x + d.y * d.y); if (len < 1e-3f) len = 1.0f; d /= len;
                ggun.pos += d * 0.012f;
            }
        }
    }
}

void toggle_drop_mode() {
    static bool prev_drop = false;
    if (ss->playing_inputs.drop && !prev_drop) {
        ss->drop_mode = !ss->drop_mode;
        ss->alerts.push_back({ss->drop_mode ? std::string("Drop mode: press 1–0 to drop") : std::string("Drop canceled"), 0.0f, 2.0f, false});
    }
    prev_drop = ss->playing_inputs.drop;
}

void handle_inventory_hotbar() {
    bool nums[10] = {ss->playing_inputs.num_row_1, ss->playing_inputs.num_row_2, ss->playing_inputs.num_row_3, ss->playing_inputs.num_row_4, ss->playing_inputs.num_row_5, ss->playing_inputs.num_row_6, ss->playing_inputs.num_row_7, ss->playing_inputs.num_row_8, ss->playing_inputs.num_row_9, ss->playing_inputs.num_row_0};
    static bool prev[10] = {false};
    for (int i = 0; i < 10; ++i) {
        if (nums[i] && !prev[i]) {
            std::size_t idx = (i == 9) ? 9 : (std::size_t)i;
            Inventory* inv = (ss->player_vid ? ss->inv_for(*ss->player_vid) : nullptr);
            if (!inv) return;
            inv->set_selected_index(idx);
            const InvEntry* ent = inv->selected_entry();
            if (ss->drop_mode) {
                if (!ent) { ss->alerts.push_back({std::string("Slot empty"), 0.0f, 1.5f, false}); }
                else if (ss->player_vid) {
                    const Entity* p = ss->entities.get(*ss->player_vid);
                    if (p) {
                        glm::vec2 place_pos = ensure_not_in_block(p->pos);
                        if (ent->kind == INV_GUN) {
                            int gspr = -1; std::string nm = "gun";
                            if (luam) { const GunInstance* gi = ss->guns.get(ent->vid); if (gi) { for (auto const& g : luam->guns()) if (g.type == gi->def_type) { nm = g.name; if (!g.sprite.empty() && g.sprite.find(':') != std::string::npos) gspr = try_get_sprite_id(g.sprite); else gspr = -1; break; } } }
                            if (ss->player_vid) { Entity* pme = ss->entities.get_mut(*ss->player_vid); if (pme && pme->equipped_gun_vid.has_value() && pme->equipped_gun_vid->id == ent->vid.id && pme->equipped_gun_vid->version == ent->vid.version) { pme->equipped_gun_vid.reset(); } }
                            if (luam && ss->player_vid) if (const GunInstance* gi = ss->guns.get(ent->vid)) if (auto* plent = ss->entities.get_mut(*ss->player_vid)) luam->call_gun_on_drop(gi->def_type, *plent);
                            ss->ground_guns.spawn(ent->vid, place_pos, gspr);
                            if (ss->player_vid) if (auto* pm = ss->metrics_for(*ss->player_vid)) pm->guns_dropped += 1;
                            inv->remove_slot(idx);
                            ss->alerts.push_back({std::string("Dropped gun: ") + nm, 0.0f, 2.0f, false});
                        } else {
                            if (const ItemInstance* inst = ss->items.get(ent->vid)) {
                                int def_type = inst->def_type; std::string nm = "item";
                                if (luam) { for (auto const& d : luam->items()) if (d.type == def_type) { nm = d.name; break; } }
                                if (inst->count > 1) {
                                    if (auto* mut = ss->items.get(ent->vid)) { mut->count -= 1; }
                                    if (auto nv = ss->items.alloc()) { if (auto* newv = ss->items.get(*nv)) { newv->active = true; newv->def_type = def_type; newv->count = 1; ss->ground_items.spawn(*nv, place_pos); } }
                                } else {
                                    ss->ground_items.spawn(ent->vid, place_pos); inv->remove_slot(idx);
                                }
                                if (ss->player_vid) if (auto* pm = ss->metrics_for(*ss->player_vid)) pm->items_dropped += 1;
                                ss->alerts.push_back({std::string("Dropped item: ") + nm, 0.0f, 2.0f, false});
                            }
                        }
                    }
                }
            } else {
                if (!ent) { /* empty */ }
                else if (ent->kind == INV_GUN) {
                    if (ss->player_vid) { if (Entity* p = ss->entities.get_mut(*ss->player_vid)) { p->equipped_gun_vid = ent->vid; if (luam) { if (const GunInstance* gi = ss->guns.get(ent->vid)) { for (auto const& g : luam->guns()) if (g.type == gi->def_type) { ss->alerts.push_back({std::string("Equipped ") + g.name, 0.0f, 1.2f, false}); break; } } } } }
                } else if (ent->kind == INV_ITEM) {
                    // just selects now
                }
            }
        }
        prev[i] = nums[i];
    }
}
