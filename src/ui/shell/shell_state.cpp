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

  // Ken Burns + crossfade (sin cambios)
  if (wallpapers.empty()) {
    updateDrag(dt);
    return;
  }

  auto animateLayer = [&](int idx) {
    if (idx < 0 || idx >= (int)wallpapers.size())
      return;
    WallpaperLayer &L = wallpapers[idx];
    float a = std::min(1.0f, 0.15f * dt);
    L.kb_scale += (cfg.wallpaper_ken_burns_zoom - L.kb_scale) * a;
  };

  if (!wp_in_transition && wp_current >= 0) {
    if (cfg.wallpaper_ken_burns)
      animateLayer(wp_current);
    wp_timer += dt;
    if (cfg.wallpaper_rotate && wp_timer >= cfg.wallpaper_interval) {
      nextWallpaper();
    }
  }

  if (wp_in_transition && wp_next >= 0) {
    if (cfg.wallpaper_ken_burns)
      animateLayer(wp_next);
    float fade_speed = (cfg.wallpaper_fade_duration > 0.001f)
                           ? 1.0f / cfg.wallpaper_fade_duration
                           : 1000.0f;
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
    id = panel_last_id; // fantasma: sigue dibujándose mientras sale
  return id;
}

float ShellState::panelEased() const {
  float t = panel_anim_t;
  if (panel_anim == PanelAnim::Closing)
    return t * t;                          // ease-in: acelera hacia abajo
  return 1.0f - (1.0f - t) * (1.0f - t);   // ease-out: frena al llegar
}

void ShellState::updatePanelAnimation(float dt) {
  int curr = openPanelId();
  constexpr int SYSTEM_ID = 6; // SOLO el panel system anima

  if (panel_anim == PanelAnim::Idle) {
    if (curr == SYSTEM_ID && panel_last_id == 0) {
      panel_anim = PanelAnim::Opening; // system entra
      panel_anim_t = 0.0f;
    } else if (curr == 0 && panel_last_id == SYSTEM_ID) {
      panel_anim = PanelAnim::Closing; // system sale a tiles
      panel_anim_t = 1.0f;
    }
  } else if (panel_anim == PanelAnim::Opening) {
    if (curr == 0)
      panel_anim = PanelAnim::Closing; // cerrado a mitad de entrada: invertir
    else if (curr != SYSTEM_ID) {      // saltó a un subpanel: sin animación
      panel_anim = PanelAnim::Idle;
      panel_anim_t = 1.0f;
    }
  } else if (panel_anim == PanelAnim::Closing) {
    if (curr == SYSTEM_ID)
      panel_anim = PanelAnim::Opening; // reabierto a mitad de salida: invertir
    else if (curr != 0) {              // abrió otro panel: sin animación
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