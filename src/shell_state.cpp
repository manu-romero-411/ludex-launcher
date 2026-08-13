#include "shell_state.h"

#include <algorithm>

void ShellState::refresh(const Config& cfg) {
    apps = discoverApps(cfg);

    if (apps.empty()) {
        selected = 0;
    } else {
        selected = std::clamp(selected, 0, static_cast<int>(apps.size()) - 1);
    }

    carousel_offset = static_cast<float>(selected);
}

void ShellState::nav(int dx) {
    if (apps.empty()) {
        return;
    }

    auto now = std::chrono::steady_clock::now();

    auto elapsed =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            now - last_nav_
        ).count();

    if (elapsed < NAV_COOLDOWN_MS) {
        return;
    }

    last_nav_ = now;

    int count = static_cast<int>(apps.size());

    selected += dx;
    selected %= count;

    if (selected < 0) {
        selected += count;
    }
}

void ShellState::update(float dt) {
    float target = static_cast<float>(selected);

    float alpha = CAROUSEL_LERP_RATE * dt;
    if (alpha > 1.0f) {
        alpha = 1.0f;
    }

    carousel_offset += (target - carousel_offset) * alpha;
}

const App* ShellState::selectedApp() const {
    if (apps.empty()) {
        return nullptr;
    }

    return &apps[selected];
}

App* ShellState::selectedApp() {
    if (apps.empty()) {
        return nullptr;
    }

    return &apps[selected];
}
