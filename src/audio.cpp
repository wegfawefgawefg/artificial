#include "audio.hpp"
#include "globals.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <system_error>

bool init_audio() {
    if (!aa) aa = new Audio();
    if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 1024) != 0)
        return false;
    return true;
}

void cleanup_audio() {
    if (!aa) return;
    for (auto& kv : aa->chunks)
        Mix_FreeChunk(kv.second);
    aa->chunks.clear();
    if (Mix_QuerySpec(nullptr, nullptr, nullptr))
        Mix_CloseAudio();
    delete aa;
    aa = nullptr;
}

bool load_sound(const std::string& key, const std::string& path) {
    if (!aa) return false;
    Mix_Chunk* ch = Mix_LoadWAV(path.c_str());
    if (!ch)
        return false;
    aa->chunks[key] = ch;
    return true;
}

void play_sound(const std::string& key, int loops, int channel, int volume) {
    if (!aa) return;
    auto it = aa->chunks.find(key);
    if (it == aa->chunks.end())
        return;
    if (volume >= 0)
        Mix_VolumeChunk(it->second, volume);
    Mix_PlayChannel(channel, it->second, loops);
}

void load_mod_sounds() {
    auto mods_root = mm->root;

    std::error_code ec;
    std::filesystem::path mroot = std::filesystem::path(mods_root);
    if (!std::filesystem::exists(mroot, ec) || !std::filesystem::is_directory(mroot, ec)) {
        return;
    }

    for (auto const& mod : std::filesystem::directory_iterator(mroot, ec)) {
        if (ec) { ec.clear(); continue; }
        if (!mod.is_directory()) continue;

        std::string modname = mod.path().filename().string();
        auto sp = mod.path() / "sounds";
        if (!std::filesystem::exists(sp, ec) || !std::filesystem::is_directory(sp, ec)) {
            continue;
        }

        for (auto const& f : std::filesystem::directory_iterator(sp, ec)) {
            if (ec) { ec.clear(); continue; }
            if (!f.is_regular_file()) continue;

            auto p = f.path();
            std::string ext = p.extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
            if (ext == ".wav" || ext == ".ogg") {
                std::string stem = p.stem().string();
                std::string key = modname + ":" + stem;
                (void)load_sound(key, p.string());
            }
        }
    }
}
