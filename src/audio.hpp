
#pragma once

#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>
#include <string>
#include <unordered_map>

// Struct-only audio store; functions operate on it.
struct Audio {
    std::unordered_map<std::string, Mix_Chunk*> chunks;
};

// Initialize SDL_mixer and allocate the global Audio instance.
bool init_audio();

// Free all loaded chunks, shutdown SDL_mixer, and destroy the global instance.
void cleanup_audio();

// Load a sound file (.wav/.ogg) into the global store with a key.
bool load_sound(const std::string& key, const std::string& path);

// Play a sound by key from the global store. Optional loops/channel/volume.
void play_sound(const std::string& key, int loops = 0, int channel = -1, int volume = -1);

// Scan mods/*/sounds for audio assets and load into global store.
void load_mod_sounds(const std::string& mods_root = "mods");

