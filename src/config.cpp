#include "config.h"

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>

#include "ini.h"

static std::vector<std::string> splitArgs(const std::string& s) {
    std::vector<std::string> out;
    std::istringstream iss(s);
    std::string cur;
    while (iss >> cur) out.push_back(cur);
    return out;
}

static std::vector<std::string> splitCsv(const std::string& s) {
    std::vector<std::string> out;
    std::istringstream iss(s);
    std::string cur;
    while (std::getline(iss, cur, ',')) {
        while (!cur.empty() && std::isspace((unsigned char)cur.front()))
            cur.erase(cur.begin());
        while (!cur.empty() && std::isspace((unsigned char)cur.back()))
            cur.pop_back();
        if (!cur.empty()) out.push_back(cur);
    }
    return out;
}

static std::string joinCsv(const std::vector<std::string>& v) {
    std::string out;
    for (size_t i = 0; i < v.size(); ++i) {
        if (i) out += ",";
        out += v[i];
    }
    return out;
}

static std::string joinArgs(const std::vector<std::string>& v) {
    std::string out;
    for (const auto& a : v) {
        if (!out.empty()) out += " ";
        out += a;
    }
    return out;
}

/* ------------------------------------------------------------------
 * Valores por defecto. Se aplican SOLO cuando el INI no existe o le
 * falta una clave. Nunca viven en el struct Config (el header está
 * limpio). Esto hace que los defaults sean visibles y editables en
 * un solo sitio.
 * ------------------------------------------------------------------ */
namespace defaults {

static std::filesystem::path appsDir() {
    if (const char* e = std::getenv("LUDEX_APPS_DIR")) return e;
    // Ruta relativa al directorio donde se ejecuta el binario.
    return std::filesystem::current_path() / "resources" / "apps";
}

static std::filesystem::path wallpaperDir() {
    if (const char* e = std::getenv("LUDEX_WALLPAPER_DIR")) return e;
    if (const char* h = std::getenv("HOME")) {
        return std::filesystem::path(h) / "Imágenes" / "fondos";
    }
    return std::filesystem::current_path() / "resources" / "wallpapers";
}

static std::string fontRegular() {
    if (const char* e = std::getenv("LUDEX_FONT_REGULAR")) return e;
    // Debian/Ubuntu comunes
    return "/usr/share/fonts/truetype/raleway/Raleway-Regular.ttf";
}

static std::string fontBold() {
    if (const char* e = std::getenv("LUDEX_FONT_BOLD")) return e;
    return "/usr/share/fonts/truetype/raleway/Raleway-Bold.ttf";
}

static std::string browserBin() {
    if (const char* e = std::getenv("LUDEX_BROWSER_BIN")) return e;
    return "google-chrome";
}

static std::vector<std::string> browserExtraArgs() {
    if (const char* e = std::getenv("LUDEX_BROWSER_EXTRA_ARGS"))
        return splitArgs(e);
    return {"--new-window"};
}

static std::vector<std::string> wallpaperExts() {
    return {".png", ".jpg", ".jpeg", ".webp"};
}

static std::vector<std::string> iconExts() {
    return {".svg", ".png", ".jpg", ".jpeg", ".webp"};
}

} // namespace defaults

/* ------------------------------------------------------------------ */

bool Config::load(const std::filesystem::path& path) {
    Ini ini;
    bool existed = ini.load(path);
    const std::string S = "ludex-launcher";

    // Rutas de datos
    apps_dir     = ini.get(S, "apps_dir",     defaults::appsDir().string());
    wallpaper_dir= ini.get(S, "wallpaper_dir",defaults::wallpaperDir().string());

    std::string we = ini.get(S, "wallpaper_exts");
    wallpaper_exts = we.empty() ? defaults::wallpaperExts() : splitCsv(we);

    std::string ie = ini.get(S, "icon_exts");
    icon_exts = ie.empty() ? defaults::iconExts() : splitCsv(ie);

    // Browser
    browser_bin = ini.get(S, "browser_bin", defaults::browserBin());
    std::string bea = ini.get(S, "browser_extra_args");
    browser_extra_args = bea.empty() ? defaults::browserExtraArgs()
                                     : splitArgs(bea);

    // Fuentes
    font_regular = ini.get(S, "font_regular", defaults::fontRegular());
    font_bold    = ini.get(S, "font_bold",    defaults::fontBold());

    // Layout
    side = ini.get(S, "side", side);
    if (side != "left" && side != "right" && side != "top" && side != "bottom") {
        side = "left";
    }
    visible_items  = ini.getInt  (S, "visible_items",  visible_items);
    tile_w_pct     = ini.getFloat(S, "tile_w_pct",     tile_w_pct);
    tile_sel_w_pct = ini.getFloat(S, "tile_sel_w_pct", tile_sel_w_pct);
    tile_sel_ratio = ini.getFloat(S, "tile_sel_ratio", tile_sel_ratio);
    menu_h_pct     = ini.getFloat(S, "menu_h_pct",     menu_h_pct);
    icon_pct       = ini.getFloat(S, "icon_pct",       icon_pct);
    icon_sel_pct   = ini.getFloat(S, "icon_sel_pct",   icon_sel_pct);
    clock_pct      = ini.getFloat(S, "clock_pct",      clock_pct);
    date_pct       = ini.getFloat(S, "date_pct",       date_pct);
    font_tile_pct  = ini.getFloat(S, "font_tile_pct",  font_tile_pct);
    edge_fade_pct   = ini.getFloat(S, "edge_fade_pct",   edge_fade_pct);
    edge_fade_alpha = ini.getFloat(S, "edge_fade_alpha", edge_fade_alpha);
    show_player_indicators = ini.getInt(S, "show_player_indicators", show_player_indicators) != 0;
    theme = ini.get(S, "theme", theme);          // load

    if (visible_items < 3) visible_items = 3;
    if (visible_items % 2 == 0) visible_items++;
    if (side != "left" && side != "right") side = "left";

    // Input
    active_player = ini.getInt(S, "active_player", 0);

    // Si el INI no existía, o le faltaban claves, lo (re)generamos
    // completo para que el usuario pueda editar todo a partir de ahí.
    if (!existed) {
        if (!save(path)) {
            std::cerr << "[ludex] no se pudo escribir INI inicial: "
                      << path << "\n";
        } else {
            std::cout << "[ludex] INI creado con valores por defecto: "
                      << path << "\n";
        }
    }

    return true;
}

bool Config::save(const std::filesystem::path& path) const {
    std::error_code ec;
    if (path.has_parent_path()) {
        std::filesystem::create_directories(path.parent_path(), ec);
    }

    Ini ini;
    const std::string S = "ludex-launcher";

    ini.set(S, "apps_dir",            apps_dir.string());
    ini.set(S, "wallpaper_dir",       wallpaper_dir.string());
    ini.set(S, "wallpaper_exts",      joinCsv(wallpaper_exts));
    ini.set(S, "icon_exts",           joinCsv(icon_exts));

    ini.set(S, "browser_bin",         browser_bin);
    ini.set(S, "browser_extra_args",  joinArgs(browser_extra_args));

    ini.set(S, "font_regular",        font_regular);
    ini.set(S, "font_bold",           font_bold);

    ini.set(S, "side",                side);
    ini.set(S, "visible_items",       std::to_string(visible_items));
    ini.set(S, "tile_w_pct",          std::to_string(tile_w_pct));
    ini.set(S, "tile_sel_w_pct",      std::to_string(tile_sel_w_pct));
    ini.set(S, "tile_sel_ratio",      std::to_string(tile_sel_ratio));
    ini.set(S, "menu_h_pct",          std::to_string(menu_h_pct));
    ini.set(S, "icon_pct",            std::to_string(icon_pct));
    ini.set(S, "icon_sel_pct",        std::to_string(icon_sel_pct));
    ini.set(S, "clock_pct",           std::to_string(clock_pct));
    ini.set(S, "date_pct",            std::to_string(date_pct));
    ini.set(S, "font_tile_pct",       std::to_string(font_tile_pct));
    ini.set(S, "edge_fade_pct",   std::to_string(edge_fade_pct));
    ini.set(S, "edge_fade_alpha", std::to_string(edge_fade_alpha));
    ini.set(S, "active_player",       std::to_string(active_player));
    ini.set(S, "theme", theme);                  // save
    ini.set(S, "show_player_indicators", show_player_indicators ? "1" : "0");
    return ini.save(path);
}

/* ------------------------------------------------------------------
 * Resolución de la ruta del INI. Orden de prioridad:
 *
 *   1. $LUDEX_INI                      (override explícito)
 *   2. ./ludex-launcher.ini            (portable: junto al binario)
 *   3. $XDG_CONFIG_HOME/ludex/...      (convención XDG)
 *   4. $HOME/.config/ludex/...
 * ------------------------------------------------------------------ */
Config loadConfig() {
    std::filesystem::path path;

    if (const char* env = std::getenv("LUDEX_INI")) {
        path = env;
    } else if (std::filesystem::exists("ludex-launcher.ini")) {
        path = std::filesystem::absolute("ludex-launcher.ini");
    } else if (const char* xdg = std::getenv("XDG_CONFIG_HOME")) {
        path = std::filesystem::path(xdg) / "ludex" / "ludex-launcher.ini";
    } else if (const char* home = std::getenv("HOME")) {
        path = std::filesystem::path(home) / ".config" / "ludex" /
               "ludex-launcher.ini";
    } else {
        path = std::filesystem::absolute("ludex-launcher.ini");
    }

    Config cfg;
    cfg.ini_path = path;
    cfg.load(path);
    return cfg;
}