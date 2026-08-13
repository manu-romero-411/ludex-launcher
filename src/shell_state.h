#pragma once

#include <chrono>
#include <vector>

#include "app_discovery.h"
#include "config.h"

struct ShellState {
    std::vector<App> apps;

    int selected = 0;
    float carousel_offset = 0.0f;

    void* wallpaper_texture = nullptr;
    int wallpaper_w = 0;
    int wallpaper_h = 0;

    void refresh(const Config& cfg);
    void nav(int dx);
    void update(float dt);

    const App* selectedApp() const;
    App* selectedApp();

private:
    std::chrono::steady_clock::time_point last_nav_{};

    static constexpr int NAV_COOLDOWN_MS = 200;
    static constexpr float CAROUSEL_LERP_RATE = 10.0f;
};
