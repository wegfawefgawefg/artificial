#include "config.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <unordered_map>
#include "globals.hpp"

static std::unordered_map<std::string, SDL_Scancode> make_scancode_map() {
    using S = SDL_Scancode;
    std::unordered_map<std::string, S> m;
    auto put = [&](const char* name, S sc) { m.emplace(name, sc); };
    // letters
    for (char c = 'A'; c <= 'Z'; ++c) {
        std::string k(1, c);
        put(k.c_str(), static_cast<S>(SDL_SCANCODE_A + (c - 'A')));
    }
    // digits top row
    put("0", SDL_SCANCODE_0);
    put("1", SDL_SCANCODE_1);
    put("2", SDL_SCANCODE_2);
    put("3", SDL_SCANCODE_3);
    put("4", SDL_SCANCODE_4);
    put("5", SDL_SCANCODE_5);
    put("6", SDL_SCANCODE_6);
    put("7", SDL_SCANCODE_7);
    put("8", SDL_SCANCODE_8);
    put("9", SDL_SCANCODE_9);
    // numpad
    put("KP_0", SDL_SCANCODE_KP_0);
    put("KP_1", SDL_SCANCODE_KP_1);
    put("KP_2", SDL_SCANCODE_KP_2);
    put("KP_3", SDL_SCANCODE_KP_3);
    put("KP_4", SDL_SCANCODE_KP_4);
    put("KP_5", SDL_SCANCODE_KP_5);
    put("KP_6", SDL_SCANCODE_KP_6);
    put("KP_7", SDL_SCANCODE_KP_7);
    put("KP_8", SDL_SCANCODE_KP_8);
    put("KP_9", SDL_SCANCODE_KP_9);
    // arrows
    put("LEFT", SDL_SCANCODE_LEFT);
    put("RIGHT", SDL_SCANCODE_RIGHT);
    put("UP", SDL_SCANCODE_UP);
    put("DOWN", SDL_SCANCODE_DOWN);
    // misc
    put("SPACE", SDL_SCANCODE_SPACE);
    put("RETURN", SDL_SCANCODE_RETURN);
    put("ESCAPE", SDL_SCANCODE_ESCAPE);
    put("BACKSPACE", SDL_SCANCODE_BACKSPACE);
    put(",", SDL_SCANCODE_COMMA);
    put(".", SDL_SCANCODE_PERIOD);
    put("-", SDL_SCANCODE_MINUS);
    put("=", SDL_SCANCODE_EQUALS);
    return m;
}

static std::string trim(const std::string& s) {
    size_t a = 0;
    while (a < s.size() && std::isspace(static_cast<unsigned char>(s[a])))
        ++a;
    size_t b = s.size();
    while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1])))
        --b;
    return s.substr(a, b - a);
}

bool load_input_bindings_from_ini(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open())
        return false;
    auto map = make_scancode_map();
    InputBindings b{};
    std::string line;
    while (std::getline(f, line)) {
        auto hash = line.find('#');
        if (hash != std::string::npos)
            line = line.substr(0, hash);
        auto eq = line.find('=');
        if (eq == std::string::npos)
            continue;
        std::string key = trim(line.substr(0, eq));
        std::string val = trim(line.substr(eq + 1));
        // uppercase val for map
        std::transform(val.begin(), val.end(), val.begin(),
                       [](unsigned char c) { return std::toupper(c); });
        auto it = map.find(val);
        if (it == map.end())
            continue;
        SDL_Scancode sc = it->second;
        if (key == "left")
            b.left = sc;
        else if (key == "right")
            b.right = sc;
        else if (key == "up")
            b.up = sc;
        else if (key == "down")
            b.down = sc;
        else if (key == "use_left")
            b.use_left = sc;
        else if (key == "use_right")
            b.use_right = sc;
        else if (key == "use_up")
            b.use_up = sc;
        else if (key == "use_down")
            b.use_down = sc;
        else if (key == "use_center")
            b.use_center = sc;
        else if (key == "pick_up")
            b.pick_up = sc;
        else if (key == "drop")
            b.drop = sc;
    }
    ss->input_binds = b;
    return true;
}
