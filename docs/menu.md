Menu System Design
==================

Goals
-----
- Provide a clear, fixed-position main menu and settings pages with no scrolling.
- Support both mouse and controller navigation seamlessly.
- Use normalized coordinates (0..1) for all UI placement and sizing.
- Keep rendering and navigation logic decoupled; rendering lays out widgets, navigation uses a graph.
- Allow multi-page variants for categories when entries exceed one page (Video 1/2, etc.).
- Simple, stateless per-frame button registration with callbacks on activation.

High-Level Structure
--------------------
- Mode: Entered when not playing (separate from in-game pages like Stage Review/Next Area).
- Menu state machine:
  - Main: Play, Settings, Quit
  - Settings: Audio, Video, Controls, Other
  - Audio: master volume, music volume, sfx volume
  - Video: resolution, window mode, vsync, frame rate limit (multi-page if needed)
  - Controls: screen shake (0..1), mouse sensitivity, controller sensitivity, invert X/Y, vibration on/off, Binds → binds page
  - Binds: list of bindable actions; select an action then press a key/button to bind
- Each page is self-contained with fixed-position controls and optional page navigation buttons (Prev/Next) at top-left or top-right.

Coordinate System & Layout
--------------------------
- All positions/sizes are normalized floats in [0, 1].
- A `RectNdc { x, y, w, h }` represents a rectangle in normalized device coordinates.
- Layout helpers compute fixed vertical lists and grids using normalized spacing:
  - `layout_vlist(top_left, item_w, item_h, vgap, count)` → array of RectNdc
  - `layout_row(start, item_w, item_h, hgap, count)` → array of RectNdc
- Safe area: optional margins (e.g., 0.05 on all sides) for consistent framing.
- Text scales derive from height `h` and global UI scale (settings value).

Widgets & Rendering
-------------------
- The render pass builds a transient `buttons` vector each frame.
- A generic `Button` widget supports label, rect, state (normal/hover/focused/pressed/disabled), and callback id.
- Additional controls are composed from `Button`:
  - Toggle: left/right buttons or a single button that cycles values
  - Slider: value in [0,1] with left/right to adjust; mouse drag on the bar
  - OptionCycle: list of discrete options; left/right to cycle; click to advance
- Rendering determines exact positions; interaction logic consumes the generated `buttons` and a separate navigation graph.

Data Model (Transient UI per Frame)
-----------------------------------
- ButtonDesc (built in render):
  - `id: int` (stable within the page)
  - `rect: RectNdc`
  - `label: string`
  - `kind: enum { Button, Toggle, Slider, OptionCycle }`
  - `value: float or int` (for Slider/OptionCycle)
  - `enabled: bool`
  - `on_activate: CallbackId` (resolves to a function)
  - `on_left: CallbackId` (for OptionCycle/Slider decrement)
  - `on_right: CallbackId` (for OptionCycle/Slider increment)
- NavNode (persistent per page):
  - `up, down, left, right: int` (neighbor ids; -1 if none)
  - Built once per page definition; independent of rendering code.
- MenuState (persistent):
  - `page: enum { Main, Settings, Audio, Video, Video2, Controls, Controls2, Binds, Other, ... }`
  - `focus_id: int` (currently focused button for controller navigation)
  - `mouse_last_used: bool` (last input method; influences hover→focus sync)
  - `page_index: int` (for multi-page categories)

Input & Interaction Model
-------------------------
- Mouse:
  - Hover: the button whose rect contains the cursor.
  - Click: activates `on_activate` for Button/Toggle; for OptionCycle advances to next; for Slider picks position/drag.
  - Hover sets focus when mouse is the last-used device.
- Controller (gamepad/keyboard navigation):
  - D-pad/Left stick: move focus using the Nav graph (up/down/left/right neighbors).
  - Activate: `A`/`Enter` triggers `on_activate`.
  - Back: `B`/`Esc` goes back to previous menu or closes settings to Main.
  - Left/Right on focused control: calls `on_left`/`on_right` for sliders/options.
  - Bumpers (LB/RB): optional shortcut to previous/next page in multi-page sections.
- Focus rules:
  - Exactly one focused control in controller mode.
  - On page enter, focus the first enabled control (or a stored last focus for the page).
  - Mouse hover transfers focus when mouse is active; controller input reclaims focus to nav graph.

Callbacks & Actions
-------------------
- Callbacks mutate settings or trigger transitions:
  - Page transitions: change `MenuState.page` and reset/select focus.
  - Setting changes: update `Settings` fields and invoke apply functions when needed.
  - Binds: enter capture mode for a selected action.
- Callback execution happens on `on_activate` (or `on_left/right` for value changes) during the UI handling step, not in rendering.

Settings Catalog
----------------
- Audio
  - Master Volume: slider [0..1]
  - Music Volume: slider [0..1]
  - SFX Volume: slider [0..1]
- Video
  - Resolution: OptionCycle over supported modes (e.g., 1920×1080); applies on confirm
  - Window Mode: OptionCycle { Windowed, Borderless, Fullscreen }
  - VSync: Toggle { Off, On }
  - Frame Rate Limit: OptionCycle over { Off, 30, 60, 120, 144, 240 } (or slider with sentinel 0=Off)
  - Pagination: If more entries are added later, split into Video (Page 1), Video (Page 2) with Prev/Next buttons at top corners.
- Controls
  - Screen Shake: slider [0..1]
  - Mouse Sensitivity: slider (e.g., 0.1..10.0 mapped to [0..1])
  - Controller Sensitivity: slider (0.1..10.0)
  - Invert X: toggle
  - Invert Y: toggle
  - Vibration: toggle
  - Binds: button → opens Binds page
- Binds
  - List of actions (e.g., Move Up/Down/Left/Right, Fire, Reload, Dash, Pickup, Drop, Inventory 1–0, Pause).
  - Selecting an action enters capture mode: next key or controller button becomes the new binding.
  - Display current binding token (SDL scancode name or controller button name).
  - Escape/B cancels capture without changes.

Application & Persistence
-------------------------
- Settings live in an engine `Settings` struct and serialize to `config/input.ini` (bindings) and `config/video.ini`/`config/audio.ini` or a unified `.ini` (TBD).
- Audio changes apply immediately to SDL_mixer channels (master/music/sfx volumes).
- Video changes:
  - Resolution/window mode: reconfigure SDL window and renderer; may require recreate.
  - VSync: toggle swap interval.
  - Frame rate cap: engine sleep/time-step limiter honors the selected cap (0=uncapped, otherwise clamp).
- Controls changes:
  - Screen shake multiplier feeds rendering effects; default 1.0.
  - Sensitivities affect camera/mouse/controller aim math; applied immediately.
  - Invert axes flips input signs; vibration toggles SDL haptics usage.
- Binds:
  - Writes to `config/input.ini` using the same token format the current `.ini` parser supports (SDL scancode names, etc.).
  - Gamepad binds stored with a distinct prefix (e.g., `PAD_A`, `PAD_LB`) to avoid collisions with keyboard tokens.

Navigation Graph Specification
------------------------------
- Each page defines a static adjacency map for controller navigation.
- Graph nodes are stable `id`s of buttons present on that page. Rendering must produce ButtonDesc entries for these ids.
- When a page is multi-page (e.g., Video 1/2), each subpage has its own graph.
- Missing neighbor (`-1`) causes input to do nothing in that direction.
- Default traversal for vertical lists: each item’s `down` points to next, `up` to previous; left/right point to sibling controls when present, else to self.

Multi-Page Behavior
-------------------
- Pagination controls appear at a fixed corner (top-right by default): `Prev` and `Next` as small buttons.
- Keyboard/controller shortcuts: LB=Prev, RB=Next.
- Focus wraps or clamps per design; recommended: clamp (no wrap) to avoid disorientation.

Visual/UX Guidelines
--------------------
- Fixed positions: main menu vertical list centered-left; settings lists left-aligned within safe area.
- Selection: focused item has a clear highlight ring; hovered also highlights with distinct style.
- Disabled: faded style and non-interactive; excluded from focus.
- Feedback: play subtle SFX on move/activate/back (respect volumes).
- Consistent spacing: use constants for item height and gaps (normalized), e.g., `ITEM_H=0.06`, `VGAP=0.02`.
- Page header at top-left; page indicator (e.g., "Video — Page 1/2") at top-right.

Example Page Layouts (Normalized)
---------------------------------
- Main Menu (vertical):
  - Play (id=100)
  - Settings (id=101)
  - Quit (id=102)
  - Nav: 100↓→101→102; up/down between neighbors; left/right self.
- Settings Hub:
  - Audio (200), Video (201), Controls (202), Other (203), Back (299)
- Audio:
  - Master (300), Music (301), SFX (302), Back (399)
- Video:
  - Resolution (400), Window Mode (401), VSync (402), Frame Limit (403), Back (499)
- Controls:
  - Screen Shake (500), Mouse Sens (501), Controller Sens (502), Invert X (503), Invert Y (504), Vibration (505), Binds (506), Back (599)
- Binds:
  - Action rows (600..699), Apply (790), Reset to Defaults (791), Back (799)

Binds Capture Flow
------------------
1) User activates an action row → state enters `capture_action_id = X`.
2) UI overlays "Press a key/button…"; input system listens for the next input event.
3) On input:
   - If `Esc/B`, cancel capture (no change).
   - Else, translate event to token (keyboard scancode or gamepad button/axis) and assign to action X.
4) Persist change in-memory; save on Apply or immediately (configurable; recommended immediate for simplicity).
5) Exit capture state and return focus to the action row.

Decoupling Rendering and Logic
------------------------------
- Rendering builds `buttons: vector<ButtonDesc>` with positions and visual state.
- Logic consumes `buttons` to:
  - Determine hover from mouse position.
  - Route controller navigation via Nav graph and `focus_id`.
  - Invoke callbacks based on activation and left/right adjustments.
- The Nav graph and callbacks are defined per page module, not in the rendering pass.

Integration with Existing Config & Input
----------------------------------------
- Reuse `config/input.ini` token scheme for keyboard binds; extend for gamepad tokens.
- Respect existing input polling and debouncing; menu uses the same input layer.
- Pickup key default F remains; binds page will edit that mapping directly.

Edge Cases & Details
--------------------
- Apply vs Immediate: audio/control changes apply immediately; video can apply immediately or on confirm; if applying immediately, warn that the screen may flicker.
- Confirm on Quit: prompt Yes/No on Quit.
- Safe defaults: escape/back always returns to previous page; from Main, Esc=Quit Confirm.
- Localization-ready: labels are keys looked up in a table; fall back to English strings.
- Accessibility: ensure focus visible without color; allow turning screen shake to 0.

Testing & QA Checklist
----------------------
- Mouse-only navigation works across all pages.
- Controller-only navigation works; focus moves as expected via graph.
- Binds capture accepts keyboard and gamepad inputs; cancel works.
- Volumes audibly change; persistence across restarts.
- Video options apply and persist; vsync/frame cap behave as expected.
- No scrolling anywhere; pagination buttons appear only when needed.
- Focus never lands on disabled/hidden controls; page transitions reset focus sanely.

Future Extensions (Non-blocking)
--------------------------------
- In-game Pause menu using the same system.
- Presets/profiles for video and controls.
- Multi-bind per action or secondary binds.
- Navigation auto-graph based on spatial layout as a fallback.

