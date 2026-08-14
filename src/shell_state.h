#pragma once

#include <vector>

#include "app_discovery.h"
#include "config.h"

struct WallpaperLayer {
  void *texture = nullptr;
  int w = 0, h = 0;
  float kb_scale = 1.0f;
  float kb_pan_x = 0.0f, kb_pan_y = 0.0f;
};

enum class RowIcon { None, Settings, Exit, Shutdown, Restart, Suspend };

struct UiIcons {
  void *settings = nullptr;
  void *exit = nullptr;
  void *shutdown = nullptr;
  void *restart = nullptr;
  void *suspend = nullptr;

  void *nav_v = nullptr;
  void *nav_h = nullptr;
  void *accept = nullptr;
  void *back = nullptr;
  void *home = nullptr;
  void *gamepad = nullptr;

  void *byIndex(RowIcon icon) const {
    switch (icon) {
      case RowIcon::Settings: return settings;
      case RowIcon::Exit: return exit;
      case RowIcon::Shutdown: return shutdown;
      case RowIcon::Restart: return restart;
      case RowIcon::Suspend: return suspend;
      default: return nullptr;
    }
  }
};

struct ShellState {
  std::vector<App> apps;

  int selected = 0;
  float offset = 0.0f;

  int menu_selected = 0;
  bool show_settings = false;
  bool show_power = false;
  int settings_focus = 0;
  int power_focus = 0;
  bool menu_open = false;
  float menu_anim = 0.0f;

  std::vector<WallpaperLayer> wallpapers;
  int wp_current = -1;
  int wp_next = -1;
  float wp_timer = 0.0f;
  float wp_fade = 1.0f;
  bool wp_in_transition = false;

  UiIcons ui_icons;

  void refresh(const Config &cfg, const BackendRegistry &backends);
  void nav(int dy);
  void navMenu(int dy);
  void update(float dt, const Config &cfg);

  const App *selectedApp() const;
  bool show_controllers = false;
  int controllers_focus = 0;
  int controller_pick_player = -1;
  int controller_pick_focus = 0;

private:
  static constexpr float LERP_RATE = 10.0f;
};

float wrapHalf(float x, int n);