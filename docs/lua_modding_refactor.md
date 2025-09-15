Lua/Modding Refactor Plan and Log

Goal
- Hide sol2 types from public headers while keeping LuaManager and global ctx.
- Bottle files under src/lua by concern: runtime, registry, api, hooks.
- Preserve behavior; zero mod/API changes now. Optional API version check later.

Steps
1) Introduce internal hooks storage (src/lua/internal_state.hpp) to hold sol::protected_function by type.
2) Remove sol members from DTOs in src/lua/defs.hpp.
3) Update registrations to populate hooks maps while still pushing DTOs into LuaManager containers.
4) Update call_* sites to use hooks_ instead of DTO members.
5) Ensure all call sites include internal_state.hpp and sol headers where needed.
6) Rebuild and validate.

Next (optional)
- Move files into src/lua/{runtime,registry,api,hooks} and rename without the luamgr_ prefix.
- Add tiny api_version check in load_mods.

Status
- Internal hooks added, calls and registrations updated, build fixing in progress.

