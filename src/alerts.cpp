#include "globals.hpp"

#include <algorithm>

void age_and_prune_alerts(float dt) {
    if (!ss) return;
    for (auto& al : ss->alerts) al.age += dt;
    ss->alerts.erase(std::remove_if(ss->alerts.begin(), ss->alerts.end(),
                                      [](const State::Alert& al) {
                                          return al.purge_eof || (al.ttl >= 0.0f && al.age > al.ttl);
                                      }),
                       ss->alerts.end());
}
