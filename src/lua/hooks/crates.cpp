#include "luamgr.hpp"
// hooks: crates
#include "lua/internal_state.hpp"
#include <sol/sol.hpp>

void LuaManager::call_crate_on_open(int crate_type, Entity& player) {
    (void)player;
    auto it = hooks_->crates.find(crate_type);
    for (auto const& c : crates_) if (c.type == crate_type) {
        if (it != hooks_->crates.end() && it->second.on_open.valid()) {
            auto r = it->second.on_open();
            if (!r.valid()) { sol::error e = r; std::fprintf(stderr, "[lua] crate on_open error: %s\n", e.what()); }
        }
        break;
    }
}
