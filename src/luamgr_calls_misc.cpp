#include "luamgr.hpp"

void LuaManager::call_crate_on_open(int crate_type, Entity& player) {
    (void)player;
    for (auto const& c : crates_) if (c.type == crate_type) {
        if (c.on_open.valid()) {
            auto r = c.on_open();
            if (!r.valid()) { sol::error e = r; std::fprintf(stderr, "[lua] crate on_open error: %s\n", e.what()); }
        }
        break;
    }
}

