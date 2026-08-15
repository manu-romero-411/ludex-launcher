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

void ShellState::navMenu(int dy) {
  menu_selected = std::clamp(menu_selected + dy, 0, 3);
}

void ShellState::updateDrag(float dt) {
    if (!has_momentum || dragging) return;
    float n = (float)apps.size();
    if (n <= 0) return;

    offset += drag_velocity * dt;
    drag_velocity *= std::pow(0.05f, dt); // decae más rápido

    if (std::fabs(drag_velocity) < 0.5f) {
        has_momentum = false;
        drag_velocity = 0.0f;
        // Snap al tile más cercano
        selected = ((int)std::round(offset) % (int)n + (int)n) % (int)n;
    }
}

void ShellState::update(float dt, const Config &cfg) {
    int n = (int)apps.size();

    // Solo LERP hacia selected cuando NO hay drag ni momentum activo.
    // Si el usuario está arrastrando o hay inercia, el offset es "suyo"
    // y no debemos interferir.
    if (n > 0 && !dragging && !has_momentum) {
        float delta = wrapHalf((float)selected - offset, n);
        float alpha = std::min(1.0f, LERP_RATE * dt);
        offset += delta * alpha;
    }

    // Menú sistema
    float target = menu_open ? 1.0f : 0.0f;
    menu_anim += (target - menu_anim) * std::min(1.0f, 12.0f * dt);
    if (std::fabs(target - menu_anim) < 0.001f) menu_anim = target;

    // Ken Burns + crossfade (sin cambios)
    if (wallpapers.empty()) { updateDrag(dt); return; }

    auto animateLayer = [&](int idx) {
        if (idx < 0 || idx >= (int)wallpapers.size()) return;
        WallpaperLayer &L = wallpapers[idx];
        float a = std::min(1.0f, 0.15f * dt);
        L.kb_scale += (cfg.wallpaper_ken_burns_zoom - L.kb_scale) * a;
    };

      if (!wp_in_transition && wp_current >= 0) {
    if (cfg.wallpaper_ken_burns) animateLayer(wp_current);
    wp_timer += dt;
    if (cfg.wallpaper_rotate && wp_timer >= cfg.wallpaper_interval) {
      nextWallpaper();
    }
  }

    if (wp_in_transition && wp_next >= 0) {
        if (cfg.wallpaper_ken_burns) animateLayer(wp_next);
        float fade_speed = (cfg.wallpaper_fade_duration > 0.001f)
                               ? 1.0f / cfg.wallpaper_fade_duration : 1000.0f;
        wp_fade -= fade_speed * dt;
        if (wp_fade <= 0.0f) {
            wp_fade = 0.0f;
            wp_current = wp_next;
            wp_next = -1;
            wp_in_transition = false;
            wp_fade = 1.0f;
            wp_timer = 0.0f;
        }
    }

    updateDrag(dt);
}

const App *ShellState::selectedApp() const {
  if (apps.empty())
    return nullptr;
  return &apps[selected];
}


void ShellState::nextWallpaper() {
  if (wallpapers.size() < 2 || wp_in_transition)
    return;
  if (wp_current < 0)
    wp_current = 0;

  int next;
  do {
    next = std::rand() % (int)wallpapers.size();
  } while (next == wp_current);

  wp_next = next;
  WallpaperLayer &L = wallpapers[wp_next];
  L.kb_scale = 1.0f;
  auto r11 = []() -> float {
    return ((float)(std::rand() % 2000) / 1000.0f) - 1.0f;
  };
  L.kb_pan_x = r11() * 0.10f;
  L.kb_pan_y = r11() * 0.10f;
  wp_in_transition = true;
  wp_fade = 1.0f;
  wp_timer = 0.0f;
}