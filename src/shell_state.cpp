#include "shell_state.h"

#include <algorithm>
#include <cmath>

float wrapHalf(float x, int n) {
    if (n <= 0) return 0.0f;
    float fn = (float)n;
    float m = std::fmod(x + fn * 0.5f, fn);
    if (m < 0.0f) m += fn;
    return m - fn * 0.5f;
}

void ShellState::refresh(const Config& cfg) {
    apps = discoverApps(cfg);
    if (apps.empty()) selected = 0;
    else selected = std::clamp(selected, 0, (int)apps.size() - 1);
    offset = (float)selected;
}

void ShellState::nav(int dy) {
    if (apps.empty()) return;

    auto now = std::chrono::steady_clock::now();
    if (std::chrono::duration_cast<std::chrono::milliseconds>(
            now - last_nav_).count() < NAV_COOLDOWN_MS) {
        return;
    }
    last_nav_ = now;

    int n = (int)apps.size();
    selected = ((selected + dy) % n + n) % n;
}

void ShellState::navMenu(int dy) {
    menu_selected = std::clamp(menu_selected + dy, 0, 2);
}

void ShellState::update(float dt) {
    int n = (int)apps.size();
    if (n == 0) return;

    float delta = wrapHalf((float)selected - offset, n);
    float alpha = std::min(1.0f, LERP_RATE * dt);
    offset += delta * alpha;
    float target = menu_open ? 1.0f : 0.0f;
    menu_anim += (target - menu_anim) * std::min(1.0f, 12.0f * dt);
    if (std::fabs(target - menu_anim) < 0.001f) menu_anim = target;
}

const App* ShellState::selectedApp() const {
    if (apps.empty()) return nullptr;
    return &apps[selected];
}