#pragma once

#include "sound.hpp"

struct Audio {
    SoundStore sounds;

    bool init() {
        return sounds.init();
    }
    void shutdown() {
        sounds.shutdown();
    }
};

