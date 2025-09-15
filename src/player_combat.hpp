// Player combat utilities.
// Responsibility: shields/reload progress, reload edges (active reload),
// trigger handling (auto/single/burst), projectile spawn, jam/unjam.
#pragma once

// Active reload edge handling (R key) and related hooks.
void update_reload_active();

// Fire control and projectile spawning (trigger, burst, recoil/spread, ammo).
void update_trigger_and_fire();

// SPACE mash to unjam, with auto-reload attempt after unjam.
void update_unjam();

// Shields regen and reload progress per-tick.
void update_shields_and_reload_progress();
