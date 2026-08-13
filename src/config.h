#pragma once

#include <filesystem>
#include <string>
#include <vector>

struct Config {
    // Rutas: SIEMPRE desde el INI (o env). Sin valores en el header.
    std::filesystem::path ini_path;
    std::filesystem::path apps_dir;
    std::filesystem::path wallpaper_dir;
    std::vector<std::string> wallpaper_exts;
    std::vector<std::string> icon_exts;
    std::string browser_bin;
    std::vector<std::string> browser_extra_args;
    std::string font_regular;
    std::string font_bold;

    // Layout: constantes universales de la UI (no dependen del dispositivo),
    // por eso SÍ llevan inicializador. El INI las pisa si están definidas.
    std::string side = "left";
    std::string theme = "dark";   // "dark" | "light"
    int visible_items = 7;
    float tile_w_pct = 0.20f;
    float tile_sel_w_pct = 0.225f;
    float tile_sel_ratio = 1.2f;
    float menu_h_pct = 0.065f;
    float icon_pct = 0.0444f;
    float icon_sel_pct = 0.0593f;
    float clock_pct = 0.07f;     // antes 0.06: más grande
    float date_pct = 0.032f;     // antes 0.028
    float font_tile_pct = 0.030f;// antes 0.026: más grande
    float font_hint_pct = 0.02f;
    float edge_fade_pct = 0.22f;     // ancho del fade (fracción de W)
    float edge_fade_alpha = 110.0f;  // intensidad 0..255
    int active_player = 0;

    bool load(const std::filesystem::path& path);
    bool save(const std::filesystem::path& path) const;
};

Config loadConfig();