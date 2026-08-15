#include "assets.h"
#include "render/renderer.h"
#include "render/texture_handle.h"
#include "shell/shell_state.h"
#include <SDL.h>
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <vector>

static bool hasAllowedExt(const std::filesystem::path &path,
                          const std::vector<std::string> &exts) {
  std::string ext = path.extension().string();
  std::transform(ext.begin(), ext.end(), ext.begin(),
                 [](unsigned char c) { return std::tolower(c); });
  return std::find(exts.begin(), exts.end(), ext) != exts.end();
}

void loadAllWallpapers(Renderer &renderer, ShellState &state, const Config &cfg,
                       int screen_w, int screen_h) {
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
    int w = 0, h = 0;
    void *raw = renderer.loadTextureFromFile(path, &w, &h, 0, nullptr, screen_w,
                                             screen_h);
    if (raw) {
      L.texture = TexturePtr(raw, TextureDeleter{&renderer});
      L.w = w;
      L.h = h;
      L.kb_scale = 1.0f;
      L.kb_pan_x = 0.0f;
      L.kb_pan_y = 0.0f;
      state.wallpapers.push_back(std::move(L));
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
  int icon_max = (int)(screen_h * cfg.icon_sel_pct * 2.0f);
  for (auto &app : state.apps) {
    if (!app.icon_path.empty()) {
      app.icon_texture =
          TexturePtr(renderer.loadTextureFromFile(
                         app.icon_path, nullptr, nullptr, icon_max,
                         app.has_icon_tint ? &app.icon_tint : nullptr),
                     TextureDeleter{&renderer});
    } else {
      app.icon_texture.reset();
    }
  }
  loadAllWallpapers(renderer, state, cfg, screen_w, screen_h);
}

void freeShellAssets(Renderer &renderer, ShellState &state) {
  (void)renderer;
  state.apps.clear();
  state.wallpapers.clear();
  state.wp_current = -1;
  state.wp_next = -1;

  // Limpiar ui_icons
  state.ui_icons.settings.reset();
  state.ui_icons.exit.reset();
  state.ui_icons.shutdown.reset();
  state.ui_icons.restart.reset();
  state.ui_icons.suspend.reset();
  state.ui_icons.nav_v.reset();
  state.ui_icons.nav_h.reset();
  state.ui_icons.accept.reset();
  state.ui_icons.back.reset();
  state.ui_icons.home.reset();
  state.ui_icons.gamepad.reset();
}

void loadUiIcons(Renderer &renderer, ShellState &state, const Config &cfg,
                 int screen_h) {
  SDL_Log("[ludex] UI icons dir: %s",
          cfg.icons_dir.empty() ? "(ninguno)" : cfg.icons_dir.string().c_str());

  UiIcons &ic = state.ui_icons;

  if (cfg.icons_dir.empty()) {
    ic.settings.reset();
    ic.exit.reset();
    ic.shutdown.reset();
    ic.restart.reset();
    ic.suspend.reset();
    ic.nav_v.reset();
    ic.nav_h.reset();
    ic.accept.reset();
    ic.back.reset();
    ic.home.reset();
    ic.gamepad.reset();
    return;
  }

  int menu_sz = (int)(screen_h * cfg.menu_h_pct * 0.6f);
  int help_sz = (int)(screen_h * 0.030f);

  auto load = [&](const std::filesystem::path &p, int sz,
                  bool tint_white) -> TexturePtr {
    std::error_code e;
    if (p.empty() || !std::filesystem::exists(p, e))
      return TexturePtr();
    TileColor white{255, 255, 255};
    return TexturePtr(
        renderer.loadTextureFromFile(p, nullptr, nullptr, sz,
                                     tint_white ? &white : nullptr),
        TextureDeleter{&renderer});
  };

  ic.settings = load(cfg.icons_dir / "settings.svg", menu_sz, true);
  ic.exit = load(cfg.icons_dir / "exit.svg", menu_sz, true);
  ic.shutdown = load(cfg.icons_dir / "shutdown.svg", menu_sz, true);
  ic.restart = load(cfg.icons_dir / "restart.svg", menu_sz, true);
  ic.suspend = load(cfg.icons_dir / "suspend.svg", menu_sz, true);
  ic.gamepad =
      load(cfg.icons_dir / "gamepad.svg", (int)(screen_h * 0.030f), true);
  ic.bluetooth = load(cfg.icons_dir / "bluetooth.svg", menu_sz, true);
  ic.gear = load(cfg.icons_dir / "gear.svg", menu_sz, true);
  ic.headset = load(cfg.icons_dir / "headset.svg", menu_sz, true);

  if (cfg.help_icons != HelpIcons::None) {
    std::filesystem::path help =
        cfg.icons_dir / "help" / helpIconsToString(cfg.help_icons);
    ic.nav_v = load(help / "d_updown.svg", help_sz, false);
    ic.nav_h = load(help / "d_leftright.svg", help_sz, false);
    ic.accept = load(help / "b_south.svg", help_sz, false);
    ic.back = load(help / "b_east.svg", help_sz, false);
    ic.home = load(help / "b_home.svg", help_sz, false);
  } else {
    ic.nav_v.reset();
    ic.nav_h.reset();
    ic.accept.reset();
    ic.back.reset();
    ic.home.reset();
  }

  int loaded = 0;
  const TexturePtr *ptrs[] = {
      &ic.settings,  &ic.exit,  &ic.shutdown, &ic.restart, &ic.suspend,
      &ic.nav_v,     &ic.nav_h, &ic.accept,   &ic.back,    &ic.home,
      &ic.bluetooth, &ic.gear,  &ic.headset};
  for (const auto *p : ptrs) {
    if (*p)
      ++loaded;
  }
  SDL_Log("[ludex] UI icons cargados: %d/13", loaded);
}