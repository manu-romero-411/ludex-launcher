#include "assets.h"
#include "render/renderer.h"
#include "render/texture_handle.h"
#include "shell/shell_state.h"
#include <SDL.h>
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <vector>

void loadShellAssets(Renderer &renderer, ShellState &state, const Config &cfg,
                     int screen_w, int screen_h) {
  // 1.1: configura la caché de iconos (carga bajo demanda)
  int icon_max = (int)(screen_h * cfg.icon_sel_pct * 2.0f);
  state.icon_cache.configure(&renderer, icon_max);

  // 1.2: descubre wallpapers y carga solo el inicial (O(2) VRAM)
  if (!state.wallpapers.hasWallpaper()) {
    state.wallpapers.discover(cfg);
    state.wallpapers.loadInitial(renderer, screen_w, screen_h);
  }
}

void freeShellAssets(Renderer &renderer, ShellState &state) {
  (void)renderer;
  state.apps.clear();
  state.icon_cache.clear();
  state.wallpapers.clear();

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
  state.ui_icons.bluetooth.reset();
  state.ui_icons.gear.reset();
  state.ui_icons.headset.reset();

  state.ui_icons.cpu.reset();
  state.ui_icons.gpu.reset();
  state.ui_icons.ram.reset();
  state.ui_icons.vram.reset();
  state.ui_icons.os.reset();
  state.ui_icons.desktop.reset();
  state.ui_icons.git_commit.reset();
  state.ui_icons.build_date.reset();

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
    ic.bluetooth.reset();
    ic.gear.reset();
    ic.headset.reset();
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

  ic.settings = load(cfg.icons_dir / "cog.svg", menu_sz, true);
  ic.exit = load(cfg.icons_dir / "exit.svg", menu_sz, true);
  ic.shutdown = load(cfg.icons_dir / "shutdown.svg", menu_sz, true);
  ic.restart = load(cfg.icons_dir / "restart.svg", menu_sz, true);
  ic.suspend = load(cfg.icons_dir / "suspend.svg", menu_sz, true);
  ic.gamepad =
      load(cfg.icons_dir / "gamepad.svg", (int)(screen_h * 0.030f), true);
  ic.bluetooth = load(cfg.icons_dir / "bluetooth.svg", menu_sz, true);
  ic.gear = load(cfg.icons_dir / "gear.svg", menu_sz, true);
  ic.headset = load(cfg.icons_dir / "headset.svg", menu_sz, true);
  // En loadUiIcons, añade (menu_sz ya está calculado):
  ic.cpu = load(cfg.icons_dir / "cpu.svg", menu_sz, true);
  ic.gpu = load(cfg.icons_dir / "monitor.svg", menu_sz, true);
  ic.ram = load(cfg.icons_dir / "memory-stick.svg", menu_sz, true);
  ic.vram = load(cfg.icons_dir / "gpu.svg", menu_sz, true);
  ic.os = load(cfg.icons_dir / "terminal.svg", menu_sz, true);
  ic.desktop = load(cfg.icons_dir / "layout-dashboard.svg", menu_sz, true);
  ic.git_commit =
      load(cfg.icons_dir / "git-commit-horizontal.svg", menu_sz, true);
  ic.build_date = load(cfg.icons_dir / "cog.svg", menu_sz, true);

  // En freeShellAssets, añade los .reset() correspondientes.
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