Stages, Pages, Metrics, and Decorations
=======================================

Overview
--------
- Two clean pages between rooms:
  - Stage Review: full-screen page after exit countdown finishes; shows metrics for the room.
  - Next Area: full-screen page with details of the next room; confirm proceeds to generate it.
- Rendering moved to `src/render.cpp`. Gameplay HUD/World renders during `MODE_PLAYING`; pages (Score Review / Next Stage) render full-screen overlays and hide crosshair, HUD bars, world, and prompts.
- Exit countdown reduced to 5s; review page input delay is short (≈0.8s) to prevent accidental skip.

Controls on Pages
-----------------
- Continue: SPACE or Left Click (after the short input delay expires).

Stage Metrics (to show on Stage Review)
---------------------------------------
- Combat:
  - Damage dealt (total)
  - Shots fired / shots hit / accuracy (%)
  - Dashes used
- Survivability:
  - Damage taken (total)
  - Damage to shields
  - Plates gained / plates lost (consumed)
  - Health damage
- Loot/Interaction:
  - Crates opened, items picked up, guns picked up
- Timing:
  - Time in stage (seconds)

Next Area Details (to show on Next Area)
----------------------------------------
- Name of the room
- Generator type (e.g., prep/arena/gauntlet)
- Difficulty [0..1] shown as 0..100%
- Optional blurb

Initial Room Rotation (looped)
-----------------------------
1) Prep Room
   - Size: 8×8; all passable tiles (no voids/walls blocking pathing)
   - Enemies: stationary dummies; optional moving targets that patrol along one axis
   - Loot: pistol and medkit on a table (ground items), placed via Lua
   - Decorations: tables/props arranged via Lua
2) Arena
   - Standard combat encounter (details TBD)
3) Gauntlet
   - Movement/pressure challenge (details TBD)

Decorations (non-interactable visuals)
--------------------------------------
- Render layer: above tiles; z-sorted with entities by Y so that objects below the player draw over and above draw under.
- No gameplay interaction; purely visual; no collision, no triggers.
- Placement unconstrained: can be positioned on any tile.

Lua API (proposed)
------------------
- Stage defs (sketch):
  - register_stage{ id, name, gen_type, difficulty, size={w,h}, spawn_pools={...}, decorations={...} }
  - on_generate(id, state): place tiles/props/enemies/items; return metadata (name, difficulty)
- Decorations:
  - register_decoration{ id, sprite, size={w,h} }
  - add_decoration{ id, x, y, z=0 } // z only for sort bias if needed; default 0

Implementation Plan (engine)
----------------------------
1) Metrics: add counters in State; accumulate during room, display on Stage Review.
2) Pages (render.cpp): Score Review shows animated metrics (one reveal per 0.2s; ticking numbers); Next Stage shows heading and continue prompt. HUD/world suppressed.
3) Rooms: introduce a simple rotation of 3 room generators; call appropriate Lua hooks.
4) Decorations: add a Decorations list with sprite id and pos; render in world pass with Y‑based sort.
