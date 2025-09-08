#pragma once

#include <string>
#include <unordered_map>
#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>

class SoundStore {
public:
  bool init() {
    if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 1024) != 0) return false;
    return true;
  }
  void shutdown() {
    for (auto& kv : chunks_) Mix_FreeChunk(kv.second);
    chunks_.clear();
    if (Mix_QuerySpec(nullptr,nullptr,nullptr)) Mix_CloseAudio();
  }
  bool load_file(const std::string& key, const std::string& path) {
    Mix_Chunk* ch = Mix_LoadWAV(path.c_str());
    if (!ch) return false;
    chunks_[key] = ch;
    return true;
  }
  void play(const std::string& key, int loops = 0, int channel = -1, int volume = -1) {
    auto it = chunks_.find(key); if (it==chunks_.end()) return;
    if (volume >= 0) Mix_VolumeChunk(it->second, volume);
    Mix_PlayChannel(channel, it->second, loops);
  }
private:
  std::unordered_map<std::string, Mix_Chunk*> chunks_;
};

