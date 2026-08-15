#pragma once
#include <SDL_stdinc.h>
#include <vector>
#include "../../core/app_discovery.h"
#include "../../core/config.h"
#include "../render/renderer.h"
#include "../render/icon_cache.h"
#include "../render/wallpaper_manager.h"
#include "../render/texture_handle.h"
#include "ui/widgets/toast.h"

enum class RowIcon {
    None,
    Settings,
    Exit,
    Shutdown,
    Restart,
    Suspend,
    Bluetooth,
    Headset,
    Gamepad,
    Gear
};

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
            case RowIcon::Settings:  return settings.get();
            case RowIcon::Exit:      return exit.get();
            case RowIcon::Shutdown:  return shutdown.get();
            case RowIcon::Restart:   return restart.get();
            case RowIcon::Suspend:   return suspend.get();
            case RowIcon::Bluetooth: return bluetooth.get();
            case RowIcon::Gear:      return gear.get();
            case RowIcon::Headset:   return headset.get();
            case RowIcon::Gamepad:   return gamepad.get();
            default:                 return nullptr;
        }
    }
};

struct DragState {
    bool down = false;
    bool drag_active = false;
    float down_x = 0.0f;
    float down_y = 0.0f;
    void reset() { *this = DragState{}; }
};
struct ShellState {
    std::vector<App> apps;
    int selected = 0;
    float offset = 0.0f;

    bool show_settings = false;
    bool show_power = false;
    int settings_focus = 0;
    int power_focus = 0;

    // NUEVO: gestores de recursos diferidos
    IconCache icon_cache;
    WallpaperManager wallpapers;

    bool show_system = false;
    int system_focus = 0;
    float system_scroll = 0.0f;

    bool anyPanelOpen() const {
        return show_settings || show_power || show_controllers || show_bluetooth ||
               show_bluetooth_scan || show_system;
    }

    UiIcons ui_icons;

    bool show_controllers = false;
    int controllers_focus = 0;
    int controller_pick_player = -1;
    int controller_pick_focus = 0;

    bool dragging = false;
    float drag_start_pos = 0.0f;
    float drag_start_offset = 0.0f;
    float drag_last_pos = 0.0f;
    Uint32 drag_last_time = 0;
    float drag_velocity = 0.0f;
    bool has_momentum = false;

    bool tile_hovered = false;
    int tile_hovered_id = -1;
    bool tile_pressed = false;
    int tile_pressed_id = -1;
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

    float panel_anim_t = 1.0f;
    float panel_anim_duration = 0.18f;
    int panel_last_id = 0;

    // NUEVO: token de invalidación de specs cacheados. Se incrementa cuando
    // cambian datos que alimentan los paneles (lista BT, mandos, escaneo…).
    int panel_cache_token = 0;
    void invalidatePanels() { ++panel_cache_token; }
    void nextWallpaper(Renderer &renderer, int screen_w, int screen_h);
    void updateDrag(float dt);
    void refresh(const Config &cfg, const BackendRegistry &backends);
    void nav(int dy);
    void update(float dt, const Config &cfg, Renderer &renderer,
                int screen_w, int screen_h);
    const App *selectedApp() const;

    enum class PanelAnim { Idle, Opening, Closing };
    PanelAnim panel_anim = PanelAnim::Idle;
    int openPanelId() const;
    int drawPanelId() const;
    float panelEased() const;
    void updatePanelAnimation(float dt);

    bool show_pin = false;
    std::string pin_mac, pin_name, pin_buffer;
    int pin_focus = 0;
    ui::ToastCenter toasts;
    float pin_scroll = 0.0f;

private:
    static constexpr float LERP_RATE = 10.0f;
};

float wrapHalf(float x, int n);