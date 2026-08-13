#pragma once

#include <chrono>
#include <vector>

#include "app_discovery.h"
#include "config.h"

struct ShellState {
    std::vector<App> apps;

    int selected = 0;
    float offset = 0.0f;   // posición visual vertical (con inercia)

    int menu_selected = 0;
    bool show_settings = false;
    bool show_power = false;
    int settings_focus = 0;
    int power_focus = 0;    
    bool menu_open = false;
    float menu_anim = 0.0f;   // 0 cerrado, 1 abierto (animado)

    void* wallpaper_texture = nullptr;
    int wallpaper_w = 0;
    int wallpaper_h = 0;

    void refresh(const Config& cfg);
    void nav(int dy);
    void navMenu(int dy);
    void update(float dt);

    const App* selectedApp() const;

private:
    std::chrono::steady_clock::time_point last_nav_{};
    static constexpr int NAV_COOLDOWN_MS = 200;
    static constexpr float LERP_RATE = 10.0f;
};

/* distancia con wrap a [-n/2, n/2] para scroll infinito */
float wrapHalf(float x, int n);