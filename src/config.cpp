#include "config.h"

#include "ini.h"
#include "util.h"
#include <cstdlib>
#include <iostream>
#include <unistd.h>

/* ------------------------------------------------------------------
 * Valores por defecto. Se aplican SOLO cuando el INI no existe o le
 * falta una clave. Nunca viven en el struct Config (el header está
 * limpio). Esto hace que los defaults sean visibles y editables en
 * un solo sitio.
 * ------------------------------------------------------------------ */
namespace defaults {

static std::filesystem::path appsDir() {
  if (const char *e = std::getenv("LUDEX_APPS_DIR"))
    return e;
  std::filesystem::path ed = util::exeDir();
  if (!ed.empty()) {
    std::filesystem::path p = ed / "apps";
    std::error_code ec;
    if (std::filesystem::is_directory(p, ec))
      return p;
  }
  return std::filesystem::current_path() / "apps";
}

static std::filesystem::path wallpaperDir() {
  if (const char *e = std::getenv("LUDEX_WALLPAPER_DIR"))
    return e;
  if (const char *h = std::getenv("HOME")) {
    return std::filesystem::path(h) / "Imágenes" / "fondos";
  }
  return std::filesystem::current_path() / "resources" / "wallpapers";
}

static std::string fontRegular() {
  if (const char *e = std::getenv("LUDEX_FONT_REGULAR"))
    return e;
  // Debian/Ubuntu comunes
  return "/usr/share/fonts/truetype/raleway/Raleway-Regular.ttf";
}

static std::string fontBold() {
  if (const char *e = std::getenv("LUDEX_FONT_BOLD"))
    return e;
  return "/usr/share/fonts/truetype/raleway/Raleway-Bold.ttf";
}

static std::vector<std::string> wallpaperExts() {
  return {".png", ".jpg", ".jpeg", ".webp"};
}

static std::vector<std::string> iconExts() {
  return {".svg", ".png", ".jpg", ".jpeg", ".webp"};
}

} // namespace defaults

/* ------------------------------------------------------------------ */

bool Config::load(const std::filesystem::path &path) {
  Ini ini;
  bool existed = ini.load(path);
  const std::string S = "ludex-launcher";

  // Rutas de datos
  apps_dir = ini.get(S, "apps_dir", defaults::appsDir().string());
  wallpaper_dir =
      ini.get(S, "wallpaper_dir", defaults::wallpaperDir().string());

  std::string we = ini.get(S, "wallpaper_exts");
  wallpaper_exts = we.empty() ? defaults::wallpaperExts() : util::splitCsv(we);

  std::string ie = ini.get(S, "icon_exts");
  icon_exts = ie.empty() ? defaults::iconExts() : util::splitCsv(ie);
  // Fuentes
  font_regular = ini.get(S, "font_regular", defaults::fontRegular());
  font_bold = ini.get(S, "font_bold", defaults::fontBold());

  // Layout
  side = ini.get(S, "side", side);
  if (side != "left" && side != "right" && side != "top" && side != "bottom") {
    side = "left";
  }
  visible_items = ini.getInt(S, "visible_items", visible_items);
  menu_h_pct = ini.getFloat(S, "menu_h_pct", menu_h_pct);
  icon_pct = ini.getFloat(S, "icon_pct", icon_pct);
  icon_sel_pct = ini.getFloat(S, "icon_sel_pct", icon_sel_pct);
  font_tile_pct = ini.getFloat(S, "font_tile_pct", font_tile_pct);
  edge_fade_pct = ini.getFloat(S, "edge_fade_pct", edge_fade_pct);
  edge_fade_alpha = ini.getFloat(S, "edge_fade_alpha", edge_fade_alpha);
  show_player_indicators =
      ini.getInt(S, "show_player_indicators", show_player_indicators) != 0;
  wallpaper_interval =
      ini.getFloat(S, "wallpaper_interval", wallpaper_interval);
  wallpaper_ken_burns =
      ini.getInt(S, "wallpaper_ken_burns", wallpaper_ken_burns ? 1 : 0) != 0;
  wallpaper_ken_burns_zoom =
      ini.getFloat(S, "wallpaper_ken_burns_zoom", wallpaper_ken_burns_zoom);
  wallpaper_fade_duration =
      ini.getFloat(S, "wallpaper_fade_duration", wallpaper_fade_duration);
  theme = ini.get(S, "theme", theme); // load
  help_icons = ini.get(S, "help_icons", help_icons);

  music_dir = ini.get(S, "music_dir", "");

  icon_vert_scale = ini.getFloat(S, "icon_vert_scale", icon_vert_scale);
  wallpaper_rotate =
      ini.getInt(S, "wallpaper_rotate", wallpaper_rotate ? 1 : 0) != 0;
  all_players_ui =
      ini.getInt(S, "all_players_ui", all_players_ui ? 1 : 0) != 0; // load
  if (help_icons != "xbox" && help_icons != "playstation" &&
      help_icons != "none")
    help_icons = "xbox";
  for (int i = 0; i < MAX_PLAYERS; ++i) {
    std::string k = "p" + std::to_string(i + 1);
    controller_guid[i] = ini.get("controllers", k + "_guid", "");
    controller_name[i] = ini.get("controllers", k + "_name", "");
  }
  icons_dir = ini.get(S, "icons_dir", "");
  {
    auto isdir = [](const std::filesystem::path &p) {
      std::error_code e;
      return std::filesystem::is_directory(p, e);
    };

    std::vector<std::filesystem::path> tries;
    if (const char *env = std::getenv("LUDEX_ICONS_DIR"))
      tries.push_back(env);
    if (!icons_dir.empty())
      tries.push_back(icons_dir);
    tries.push_back(std::filesystem::current_path() / "resources" / "icons");

    std::filesystem::path ed = util::exeDir();
    if (!ed.empty()) {
      tries.push_back(ed / "resources" / "icons");        // install layout
      tries.push_back(ed / ".." / "resources" / "icons"); // dev layout (build/)
    }

    icons_dir.clear();
    for (const auto &t : tries) {
      if (isdir(t)) {
        icons_dir = std::filesystem::absolute(t);
        break;
      }
    }
  }

  if (visible_items < 3)
    visible_items = 3;
  if (visible_items % 2 == 0)
    visible_items++;

  // Input
  active_player = ini.getInt(S, "active_player", 0);

  // Si el INI no existía, o le faltaban claves, lo (re)generamos
  // completo para que el usuario pueda editar todo a partir de ahí.
  if (!existed) {
    if (!save(path)) {
      std::cerr << "[ludex] no se pudo escribir INI inicial: " << path << "\n";
    } else {
      std::cout << "[ludex] INI creado con valores por defecto: " << path
                << "\n";
    }
  }

  return true;
}

bool Config::save(const std::filesystem::path &path) const {
  std::error_code ec;
  if (path.has_parent_path()) {
    std::filesystem::create_directories(path.parent_path(), ec);
  }

  Ini ini;
  const std::string S = "ludex-launcher";

  ini.set(S, "apps_dir", apps_dir.string());
  ini.set(S, "wallpaper_dir", wallpaper_dir.string());
  ini.set(S, "wallpaper_exts", util::joinCsv(wallpaper_exts));
  ini.set(S, "icon_exts", util::joinCsv(icon_exts));

  ini.set(S, "font_regular", font_regular);
  ini.set(S, "font_bold", font_bold);

  ini.set(S, "side", side);
  ini.set(S, "visible_items", std::to_string(visible_items));
  ini.set(S, "menu_h_pct", std::to_string(menu_h_pct));
  ini.set(S, "icon_pct", std::to_string(icon_pct));
  ini.set(S, "icon_sel_pct", std::to_string(icon_sel_pct));
  ini.set(S, "font_tile_pct", std::to_string(font_tile_pct));
  ini.set(S, "edge_fade_pct", std::to_string(edge_fade_pct));
  ini.set(S, "edge_fade_alpha", std::to_string(edge_fade_alpha));
  ini.set(S, "active_player", std::to_string(active_player));
  ini.set(S, "theme", theme); // save
  ini.set(S, "show_player_indicators", show_player_indicators ? "1" : "0");
  ini.set(S, "wallpaper_interval", std::to_string(wallpaper_interval));
  ini.set(S, "wallpaper_ken_burns", wallpaper_ken_burns ? "1" : "0");
  ini.set(S, "wallpaper_ken_burns_zoom",
          std::to_string(wallpaper_ken_burns_zoom));
  ini.set(S, "wallpaper_fade_duration",
          std::to_string(wallpaper_fade_duration));
  ini.set(S, "help_icons", help_icons);
  if (!icons_dir.empty())
    ini.set(S, "icons_dir", icons_dir.string());
  ini.set(S, "all_players_ui", all_players_ui ? "1" : "0");
  ini.set(S, "icon_vert_scale", std::to_string(icon_vert_scale));
  for (int i = 0; i < MAX_PLAYERS; ++i) {
    std::string k = "p" + std::to_string(i + 1);
    ini.set("controllers", k + "_guid", controller_guid[i]);
    ini.set("controllers", k + "_name", controller_name[i]);
  }
  ini.set(S, "wallpaper_rotate", wallpaper_rotate ? "1" : "0");

  if (!music_dir.empty()) {
    ini.set(S, "music_dir", music_dir.string());
  }

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

  if (const char *env = std::getenv("LUDEX_INI")) {
    path = env;
  } else if (std::filesystem::exists("ludex-launcher.ini")) {
    path = std::filesystem::absolute("ludex-launcher.ini");
  } else if (const char *xdg = std::getenv("XDG_CONFIG_HOME")) {
    path = std::filesystem::path(xdg) / "ludex" / "ludex-launcher.ini";
  } else if (const char *home = std::getenv("HOME")) {
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