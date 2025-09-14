#pragma once

#include "crates.hpp"
#include "entities.hpp"
#include "guns.hpp"
#include "input.hpp"
#include "inventory.hpp"
#include "items.hpp"
#include "particles.hpp"
#include "pickups.hpp"
#include "projectiles.hpp"
#include "stage.hpp"
#include "types.hpp"
#include "runtime_settings.hpp"

#include <cstdint>
#include <glm/glm.hpp>
#include <optional>
#include <unordered_map>
#include <string>
#include <vector>
#include "input_defs.hpp"

struct State {
    bool running{true};
    int mode{ids::MODE_TITLE};

    // Time
    double now{0.0};
    float dt{0.0}; // in seconds
    float time_since_last_update{0.0f};
    uint32_t scene_frame{0};
    uint32_t frame{0};

    // Input
    bool mouse_mode{true};
    MouseInputs mouse_inputs = MouseInputs{};
    MenuInputs menu_inputs = MenuInputs{};
    MenuInputDebounceTimers menu_input_debounce_timers = MenuInputDebounceTimers{};
    PlayingInputs playing_inputs = PlayingInputs{};
    PlayingInputDebounceTimers playing_input_debounce_timers = PlayingInputDebounceTimers{};

    InputState input_state = InputState{};
    InputBindings input_binds = InputBindings{};

    Settings settings;


    bool game_over{false};
    bool pause{false};
    bool win{false};
    
    uint32_t points{0};
    uint32_t deaths{0};
    uint32_t frame_pause{0};

    Entities entities{};
    std::optional<VID> player_vid{};
    Particles particles{};
    Stage stage{64, 36};
    Inventory inventory = Inventory::make(); // legacy: use per-entity via inv_for()
    ItemsPool items{};
    PickupsPool pickups{};
    GroundItemsPool ground_items{};
    GunsPool guns{};
    GroundGunsPool ground_guns{};
    CratesPool crates{};
    int default_crate_type{0};
    // Projectiles (moved into State)
    Projectiles projectiles{};

    // Firing cooldown (seconds)
    float gun_cooldown{0.0f};
    // Jam base chance additive
    float base_jam_chance{0.02f};

    bool rebuild_render_texture{true};
    float cloud_density{0.5f};

    bool camera_follow_enabled{true};

    // Inventory drop mode: press Q to enter, then 1-0 to drop a slot
    bool drop_mode{false};

    // Room / flow
    glm::ivec2 start_tile{-1, -1};
    glm::ivec2 exit_tile{-1, -1};
    float exit_countdown{-1.0f};   // <0 inactive, else seconds remaining
    float score_ready_timer{0.0f}; // seconds until input allowed on score screen

    // Alerts/messages (debug or UX). Rendered top-left. Age and purge.
    struct Alert {
        std::string text;
        float age{0.0f};
        float ttl{2.0f};
        bool purge_eof{false};
    };
    std::vector<Alert> alerts;

    // FX: reticle shake amount (pixels). Attenuates per frame.
    float reticle_shake{0.0f};

    // Dash state (shift)
    float dash_timer{0.0f};
    glm::vec2 dash_dir{1.0f, 0.0f};
    // Rechargeable dash stocks
    int dash_max{1};
    int dash_stocks{1};
    float dash_refill_timer{0.0f};

    // UI FX: right-side gun panel shake (pixels)
    float gun_panel_shake{0.0f};
    float hp_bar_shake{0.0f};
    float reload_bar_shake{0.0f};

    // UI: inventory hover tracking for center info panel
    int inv_hover_index{-1};
    float inv_hover_time{0.0f};
    // UI: inventory drag-and-drop state
    bool inv_dragging{false};
    int inv_drag_src{-1};
    // UI: gun panel visibility toggle (V key)
    bool show_gun_panel{true};

    // Character panel (left) and gun panel (right)
    bool show_character_panel{false};
    float character_panel_slide{0.0f}; // 0..1
    // Short input suppression window (e.g., after page transitions) to avoid accidental fire
    float input_lockout_timer{0.0f};
    float pickup_lockout{0.0f};

    // Metrics (per-stage). Designed for multi-player: per-player metrics keyed by entity slots.
    struct PlayerMetrics {
        bool active{false};
        uint32_t version{0}; // to validate VID
        // Combat/accuracy
        uint32_t shots_fired{0};
        uint32_t shots_hit{0};
        uint32_t enemies_slain{0};
        // Reloads
        uint32_t reloads{0};
        uint32_t active_reload_success{0};
        uint32_t active_reload_fail{0};
        // Jams
        uint32_t jams{0};
        uint32_t unjam_mashes{0};
        // Damage
        std::uint64_t damage_dealt{0};
        std::uint64_t damage_taken_hp{0};
        std::uint64_t damage_taken_shield{0};
        uint32_t plates_gained{0};
        uint32_t plates_consumed{0};
        // Mobility
        uint32_t dashes_used{0};
        float dash_distance{0.0f};
        // Loot
        uint32_t powerups_picked{0};
        uint32_t items_picked{0};
        uint32_t guns_picked{0};
        uint32_t items_dropped{0};
        uint32_t guns_dropped{0};
    };

    struct StageMetrics {
        // Global
        float time_in_stage{0.0f};
        uint32_t enemies_slain{0};
        std::unordered_map<int, uint32_t> enemies_slain_by_type;
        uint32_t crates_opened{0};
        // Totals for missed calculations (optional to fill at generation time)
        uint32_t crates_spawned{0};
        uint32_t powerups_spawned{0};
        uint32_t items_spawned{0};
        uint32_t guns_spawned{0};
        // Per-player array sized to Entities::MAX; index by VID.id, validate version
        std::vector<PlayerMetrics> per_player;

        void reset(std::size_t max_players) {
            time_in_stage = 0.0f;
            enemies_slain = 0;
            enemies_slain_by_type.clear();
            crates_opened = 0;
            crates_spawned = 0;
            powerups_spawned = 0;
            items_spawned = 0;
            guns_spawned = 0;
            per_player.clear();
            per_player.resize(max_players);
        }
    } metrics{};

    // Stage review animation state
    struct ReviewStat {
        std::string label;
        double target{0.0};
        double value{0.0};
        bool header{false}; // if true, show label only, no counting
        bool done{false};
    };
    std::vector<ReviewStat> review_stats;
    float review_next_stat_timer{0.0f};
    float review_number_tick_timer{0.0f};
    std::size_t review_revealed{0};

    // Helpers to fetch per-player metrics by VID
    PlayerMetrics* metrics_for(VID v) {
        if (metrics.per_player.empty())
            metrics.per_player.resize(Entities::MAX);
        if (v.id >= metrics.per_player.size())
            metrics.per_player.resize(v.id + 1);
        auto& m = metrics.per_player[v.id];
        if (!m.active || m.version != v.version) {
            m = PlayerMetrics{};
            m.active = true;
            m.version = v.version;
        }
        return &m;
    }

    // Per-entity inventories
    std::vector<Inventory> inventories; // index by VID.id; validate by Active entity

    Inventory* inv_for(VID v) {
        if (v.id >= inventories.size()) inventories.resize(v.id + 1);
        return &inventories[v.id];
    }
    const Inventory* inv_for(VID v) const {
        if (v.id >= inventories.size()) return nullptr;
        return &inventories[v.id];
    }
};

bool init_state();
void cleanup_state();
