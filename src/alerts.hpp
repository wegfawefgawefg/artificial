// Alert utilities.
// Responsibility: aging and pruning ephemeral UI alerts each frame.
#pragma once

#include <cstddef>

// Age and prune alerts using provided dt seconds.
void age_and_prune_alerts(float dt);
