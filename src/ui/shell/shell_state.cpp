#include "shell_state.h"
#include <algorithm>
#include <cmath>

float wrapHalf(float x, int n) {
    if (n <= 0)
        return 0.0f;
    float fn = (float)n;
    float m = std::fmod(x + fn * 0.5f, fn);
    if (m < 0.0f)
        m += fn;
    return m - fn * 0.5f;
}

void ShellState::refresh(const Config &cfg, const BackendRegistry &backends) {
    apps = discoverApps(cfg, backends);
    if (apps.empty())
        selected = 0;
    else
        selected = std::clamp(selected, 0, (int)apps.size() - 1);
    offset = (float)selected;
}

void ShellState::nav(int dy) {
    if (apps.empty())
        return;
    int n = (int)apps.size();
    selected = ((selected + dy) % n + n) % n;
}

void ShellState::updateDrag(float dt) {
    if (!has_momentum || dragging)
        return;
    float n = (float)apps.size();
    if (n <= 0)
        return;
    offset += drag_velocity * dt;
    drag_velocity *= std::pow(0.05f, dt);
    if (std::fabs(drag_velocity) < 0.5f) {
        has_momentum = false;
        drag_velocity = 0.0f;
        selected = ((int)std::round(offset) % (int)n + (int)n) % (int)n;
    }
}

void ShellState::update(float dt, const Config &cfg, Renderer &renderer,
                        int screen_w, int screen_h) {
    int n = (int)apps.size();

    if (n > 0 && !dragging && !has_momentum) {
        float delta = wrapHalf((float)selected - offset, n);
        float alpha = std::min(1.0f, LERP_RATE * dt);
        offset += delta * alpha;
    }

    wallpapers.update(dt, cfg, renderer, screen_w, screen_h);

    updateDrag(dt);
}

const App *ShellState::selectedApp() const {
    if (apps.empty())
        return nullptr;
    return &apps[selected];
}

void ShellState::nextWallpaper(Renderer &renderer, int screen_w, int screen_h) {
    wallpapers.forceNext(renderer, screen_w, screen_h);
}

int ShellState::openPanelId() const {
    if (show_settings) return 1;
    if (show_power) return 2;
    if (show_controllers) return 3;
    if (show_bluetooth) return 4;
    if (show_bluetooth_scan) return 5;
    if (show_system) return 6;
    return 0;
}

int ShellState::drawPanelId() const {
    int id = openPanelId();
    if (id == 0 && panel_anim == PanelAnim::Closing)
        id = panel_last_id;
    return id;
}

float ShellState::panelEased() const {
    float t = panel_anim_t;
    if (panel_anim == PanelAnim::Closing)
        return t * t;
    return 1.0f - (1.0f - t) * (1.0f - t);
}

void ShellState::updatePanelAnimation(float dt) {
    int curr = openPanelId();
    constexpr int SYSTEM_ID = 6;

    if (panel_anim == PanelAnim::Idle) {
        if (curr == SYSTEM_ID && panel_last_id == 0) {
            panel_anim = PanelAnim::Opening;
            panel_anim_t = 0.0f;
        } else if (curr == 0 && panel_last_id == SYSTEM_ID) {
            panel_anim = PanelAnim::Closing;
            panel_anim_t = 1.0f;
        }
    } else if (panel_anim == PanelAnim::Opening) {
        if (curr == 0)
            panel_anim = PanelAnim::Closing;
        else if (curr != SYSTEM_ID) {
            panel_anim = PanelAnim::Idle;
            panel_anim_t = 1.0f;
        }
    } else if (panel_anim == PanelAnim::Closing) {
        if (curr == SYSTEM_ID)
            panel_anim = PanelAnim::Opening;
        else if (curr != 0) {
            panel_anim = PanelAnim::Idle;
            panel_anim_t = 1.0f;
        }
    }

    float speed = 1.0f / panel_anim_duration;
    if (panel_anim == PanelAnim::Opening) {
        panel_anim_t += speed * dt;
        if (panel_anim_t >= 1.0f) { panel_anim_t = 1.0f; panel_anim = PanelAnim::Idle; }
    } else if (panel_anim == PanelAnim::Closing) {
        panel_anim_t -= speed * dt;
        if (panel_anim_t <= 0.0f) { panel_anim_t = 0.0f; panel_anim = PanelAnim::Idle; }
    }

    if (curr != 0)
        panel_last_id = curr;
    else if (panel_anim == PanelAnim::Idle)
        panel_last_id = 0;
}