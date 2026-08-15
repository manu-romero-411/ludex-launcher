#pragma once

#include <SDL_stdinc.h>
#include <vector>

#include "../../core/app_discovery.h"
#include "../../core/config.h"
#include "../render/renderer.h"

struct WallpaperLayer {
  TexturePtr texture;
  int w = 0, h = 0;
  float kb_scale = 1.0f;
  float kb_pan_x = 0.0f, kb_pan_y = 0.0f;
};

enum class RowIcon { None, Settings, Exit, Shutdown, Restart, Suspend, Bluetooth, Headset, Gamepad, Gear };

struct UiIcons {
  TexturePtr settings;
  TexturePtr exit;
  TexturePtr shutdown;
  TexturePtr restart;
  TexturePtr suspend;
  TexturePtr nav_v;
  TexturePtr nav_h;
  TexturePtr accept;
  TexturePtr back;
  TexturePtr home;
  TexturePtr gamepad;
  TexturePtr bluetooth;
  TexturePtr gear;
  TexturePtr headset;

  void *byIndex(RowIcon icon) const {
    switch (icon) {
    case RowIcon::Settings:
      return settings.get();
    case RowIcon::Exit:
      return exit.get();
    case RowIcon::Shutdown:
      return shutdown.get();
    case RowIcon::Restart:
      return restart.get();
    case RowIcon::Suspend:
      return suspend.get();
    case RowIcon::Bluetooth:
      return bluetooth.get();
    case RowIcon::Gear:
      return gear.get();
    case RowIcon::Headset:
      return headset.get();
    case RowIcon::Gamepad:
      return gamepad.get();
    default:
      return nullptr;
    }
  }
};

struct DragState {
  bool mouse_down = false;
  bool drag_active = false;
  float mouse_down_x = 0.0f;
  float mouse_down_y = 0.0f;

  bool finger_down = false;
  bool touch_drag_active = false;
  float finger_down_x = 0.0f;
  float finger_down_y = 0.0f;

  void reset() { *this = DragState{}; }
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
  // Drag/touch state for carousel
  bool dragging = false;
  float drag_start_pos = 0.0f;
  float drag_start_offset = 0.0f;
  float drag_last_pos = 0.0f;
  Uint32 drag_last_time = 0;
  float drag_velocity = 0.0f;
  bool has_momentum = false;
  void updateDrag(float dt);

  bool tile_hovered = false;
  int tile_hovered_id = -1;
  bool tile_pressed = false;
  int tile_pressed_id = -1;
  void nextWallpaper();
  int pending_launch = -1;

  bool show_bluetooth = false;
  int bluetooth_focus = 0;
  bool show_bluetooth_scan = false;
  int bluetooth_scan_focus = 0;
  float settings_scroll = 0.0f;
  float power_scroll = 0.0f;
  float controllers_scroll = 0.0f;
  float bluetooth_scroll = 0.0f;
  float bluetooth_scan_scroll = 0.0f;

private:
  static constexpr float LERP_RATE = 10.0f;
};

float wrapHalf(float x, int n);
