// Pickups and inventory helpers.
// Responsibility: auto/manual pickups, separating ground items/guns,
// drop mode toggle, and number-row hotbar actions.
#pragma once

// Auto-pickup powerups on overlap with player.
void auto_pickup_powerups();

// Manual pickup handling and gentle ground separation.
void handle_manual_pickups();
void separate_ground_items();

// Drop-mode toggle and number row hotbar actions.
void toggle_drop_mode();
void handle_inventory_hotbar();
