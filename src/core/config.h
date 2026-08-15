#pragma once
#include <filesystem>
#include <string>
#include <vector>

// --- Tipos fuertes ---
enum class LayoutSide { Left, Right, Top, Bottom };
enum class Theme { Dark, Light };
enum class HelpIcons { Xbox, PlayStation, None };

// Conversión string <-> enum (para leer/escribir el INI)
LayoutSide parseSide(const std::string& s);
std::string sideToString(LayoutSide v);
Theme parseTheme(const std::string& s);
std::string themeToString(Theme v);
HelpIcons parseHelpIcons(const std::string& s);
std::string helpIconsToString(HelpIcons v);

// Helpers de consulta
inline bool isHorizontal(LayoutSide s) { return s == LayoutSide::Top || s == LayoutSide::Bottom; }
inline bool isLight(Theme t) { return t == Theme::Light; }

struct Config {
    // Rutas
    std::filesystem::path ini_path;
    std::filesystem::path apps_dir;
    std::filesystem::path wallpaper_dir;
    std::vector<std::string> wallpaper_exts;
    std::vector<std::string> icon_exts;
    std::string font_regular;
    std::string font_bold;

    // Layout (tipos fuertes)
    LayoutSide side = LayoutSide::Left;
    Theme theme = Theme::Dark;
    HelpIcons help_icons = HelpIcons::Xbox;

    int visible_items = 7;
    float tile_w_pct = 0.20f;
    float tile_sel_w_pct = 0.23f;
    float tile_sel_ratio = 1.2f;
    float menu_h_pct = 0.065f;
    float icon_pct = 0.0444f;
    float icon_sel_pct = 0.0593f;
    float clock_pct = 0.082f;
    float date_pct = 0.032f;
    float font_tile_pct = 0.030f;
    float font_hint_pct = 0.02f;
    float edge_fade_pct = 0.22f;
    float edge_fade_alpha = 110.0f;
    float icon_vert_scale = 0.64f;
    int active_player = 0;
    bool show_player_indicators = true;
    float wallpaper_interval = 60.0f;
    bool wallpaper_rotate = false;
    bool wallpaper_ken_burns = true;
    float wallpaper_ken_burns_zoom = 1.12f;
    float wallpaper_fade_duration = 2.0f;
    std::filesystem::path icons_dir;
    bool all_players_ui = true;

    bool load(const std::filesystem::path &path);
    bool save(const std::filesystem::path &path) const;

    static constexpr int MAX_PLAYERS = 8;
    std::string controller_guid[MAX_PLAYERS];
    std::string controller_name[MAX_PLAYERS];
    std::filesystem::path music_dir;
    std::string language;
};

Config loadConfig();