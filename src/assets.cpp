#include "assets.h"

#include <SDL.h>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <vector>

#include "renderer.h"
#include "shell_state.h"

static bool hasAllowedExt(const std::filesystem::path &path,
                          const std::vector<std::string> &exts) {
  std::string ext = path.extension().string();
  std::transform(ext.begin(), ext.end(), ext.begin(),
                 [](unsigned char c) { return std::tolower(c); });
  return std::find(exts.begin(), exts.end(), ext) != exts.end();
}

void loadAllWallpapers(Renderer &renderer, ShellState &state, const Config &cfg,
                       int screen_w, int screen_h) {
  // Liberar lo anterior
  for (auto &L : state.wallpapers) {
    if (L.texture)
      renderer.freeTexture(L.texture);
  }
  state.wallpapers.clear();
  state.wp_current = -1;
  state.wp_next = -1;
  state.wp_in_transition = false;
  state.wp_fade = 1.0f;
  state.wp_timer = 0.0f;

  std::error_code ec;
  if (!std::filesystem::is_directory(cfg.wallpaper_dir, ec)) {
    SDL_Log("[ludex] wallpaper_dir no es un directorio: %s",
            cfg.wallpaper_dir.string().c_str());
    return;
  }

  std::vector<std::filesystem::path> candidates;
  for (const auto &e :
       std::filesystem::directory_iterator(cfg.wallpaper_dir, ec)) {
    if (e.is_regular_file(ec) && hasAllowedExt(e.path(), cfg.wallpaper_exts)) {
      candidates.push_back(e.path());
    }
  }
  std::sort(candidates.begin(), candidates.end());

  SDL_Log("[ludex] %d candidatos de wallpaper en %s", (int)candidates.size(),
          cfg.wallpaper_dir.string().c_str());

  for (const auto &path : candidates) {
    WallpaperLayer L;
    L.texture =
        renderer.loadTextureFromFile(path, &L.w, &L.h, 0, nullptr, screen_w,
                                     screen_h); // cover a pantalla: VRAM justa
    if (L.texture) {
      L.kb_scale = 1.0f;
      L.kb_pan_x = 0.0f;
      L.kb_pan_y = 0.0f;
      state.wallpapers.push_back(L);
    }
  }

  if (!state.wallpapers.empty()) {
    state.wp_current = std::rand() % (int)state.wallpapers.size();
    state.wp_timer = 0.0f;
  }
  SDL_Log("[ludex] %d wallpapers cargados (pantalla %dx%d)",
          (int)state.wallpapers.size(), screen_w, screen_h);
}

void loadShellAssets(Renderer &renderer, ShellState &state, const Config &cfg,
                     int screen_w, int screen_h) {
  // Iconos de apps
  int icon_max = (int)(screen_h * cfg.icon_sel_pct * 2.0f);

  for (auto &app : state.apps) {
    if (app.icon_texture) {
      renderer.freeTexture(app.icon_texture);
      app.icon_texture = nullptr;
    }
    if (!app.icon_path.empty()) {
      app.icon_texture = renderer.loadTextureFromFile(
          app.icon_path, nullptr, nullptr, icon_max,
          app.has_icon_tint ? &app.icon_tint : nullptr);
    }
  }

  // >>> ESTA llamada es la que faltaba en tu archivo <<<
  loadAllWallpapers(renderer, state, cfg, screen_w, screen_h);
}

void freeShellAssets(Renderer &renderer, ShellState &state) {
  for (auto &app : state.apps) {
    if (app.icon_texture) {
      renderer.freeTexture(app.icon_texture);
      app.icon_texture = nullptr;
    }
  }
  for (auto &L : state.wallpapers) {
    if (L.texture)
      renderer.freeTexture(L.texture);
  }

  state.wallpapers.clear();
  state.wp_current = -1;
  state.wp_next = -1;
}
void loadUiIcons(Renderer &renderer, ShellState &state, const Config &cfg,
                 int screen_h) {
  SDL_Log("[ludex] UI icons dir: %s",
          cfg.icons_dir.empty() ? "(ninguno)" : cfg.icons_dir.string().c_str());

  UiIcons &ic = state.ui_icons;

  auto fr = [&](void *&t) {
    if (t) {
      renderer.freeTexture(t);
      t = nullptr;
    }
  };
  fr(ic.settings);
  fr(ic.exit);
  fr(ic.shutdown);
  fr(ic.restart);
  fr(ic.suspend);
  fr(ic.nav_v);
  fr(ic.nav_h);
  fr(ic.accept);
  fr(ic.back);
  fr(ic.home);
  fr(ic.gamepad);
  if (cfg.icons_dir.empty())
    return;

  TileColor white{255, 255, 255}; // se tintan al dibujar según tema
  int menu_sz = (int)(screen_h * cfg.menu_h_pct * 0.6f);
  int help_sz = (int)(screen_h * 0.030f);

  auto load = [&](const std::filesystem::path &p, int sz,
                  bool tint_white) -> void * {
    std::error_code e;
    if (p.empty() || !std::filesystem::exists(p, e))
      return nullptr;
    TileColor white{255, 255, 255};
    return renderer.loadTextureFromFile(p, nullptr, nullptr, sz,
                                        tint_white ? &white : nullptr);
  };

  // Menú / paneles: siluetas monocolor -> tinte blanco (se recolorean por tema)
  ic.settings = load(cfg.icons_dir / "settings.svg", menu_sz, true);
  ic.exit = load(cfg.icons_dir / "exit.svg", menu_sz, true);
  ic.shutdown = load(cfg.icons_dir / "shutdown.svg", menu_sz, true);
  ic.restart = load(cfg.icons_dir / "restart.svg", menu_sz, true);
  ic.suspend = load(cfg.icons_dir / "suspend.svg", menu_sz, true);
  ic.gamepad =
      load(cfg.icons_dir / "gamepad.svg", (int)(screen_h * 0.030f), true);
  // Ayuda: glyphs de DOS colores -> colores originales, sin tinte
  if (cfg.help_icons != "none") {
    std::filesystem::path help = cfg.icons_dir / "help" / cfg.help_icons;
    ic.nav_v = load(help / "d_updown.svg", help_sz, false);
    ic.nav_h = load(help / "d_leftright.svg", help_sz, false);
    ic.accept = load(help / "b_south.svg", help_sz, false);
    ic.back = load(help / "b_east.svg", help_sz, false);
    ic.home = load(help / "b_home.svg", help_sz, false);
  }
  int loaded = 0;
  for (void *t : {ic.settings, ic.exit, ic.shutdown, ic.restart, ic.suspend,
                  ic.nav_v, ic.nav_h, ic.accept, ic.back, ic.home}) {
    if (t)
      ++loaded;
  }
  SDL_Log("[ludex] UI icons cargados: %d/10", loaded);
}