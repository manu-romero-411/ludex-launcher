#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "config.h"

struct TileColor {
    unsigned char r = 40, g = 40, b = 46;
};

struct App {
    std::string name;
    std::string type;// webapp|exec|steam|kodi|heroic|esde|batoes
    std::string run;
    std::vector<std::string> cmd;

    std::filesystem::path icon_path;
    void* icon_texture = nullptr;

    TileColor icon_tint{255, 255, 255};  // color de tinte
    bool has_icon_tint = false;          // false = icono tal cual

    int tile_type = 0;
    TileColor color1;
    TileColor color2;
};
std::vector<App> discoverApps(const Config& cfg);