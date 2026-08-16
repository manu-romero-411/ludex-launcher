#include "shell_orchestrator.h"
#include "panels/pan_bluetooth.h"
#include "panels/pan_bluetooth_scan.h"
#include "panels/pan_controllers.h"
#include "panels/pan_settings.h"
#include "panels/pan_shutdown.h"
#include "panels/panel_renderer.h"
#include "ui/panels/pan_about.h"
#include "ui/panels/pan_mainmenu.h"
#include "ui/panels/pan_pin.h"
#include "widgets/clock.h"
#include "widgets/edge_fades.h"
#include "widgets/help_hints.h"
#include "widgets/player_indicators.h"
#include "widgets/tile_carousel.h"
#include <imgui.h>

namespace {
ui::widgets::Clock g_clock;
ui::widgets::EdgeFades g_edge_fades;
ui::widgets::HelpHints g_help_hints;
ui::widgets::PlayerIndicators g_player_indicators;
ui::widgets::TileCarousel g_tile_carousel;

// NUEVO: caché de PanelSpec para no reconstruirlo cada frame.
struct PanelCache {
  int panel_id = -1;
  int token = -1;
  ui::panels::PanelSpec spec;
};
PanelCache g_panel_cache;
PanelCache g_pin_cache;

ui::panels::PanelSpec buildPanelSpec(int id, ShellState &state, Config &cfg,
                                     const ShellActions &actions) {
  switch (id) {
  case 1:
    return ui::panels::makeSettingsPanelSpec(state, cfg, actions);
  case 2:
    return ui::panels::makeShutdownPanelSpec(state, actions);
  case 3:
    return ui::panels::makeControllersPanelSpec(state, cfg, actions);
  case 4:
    return ui::panels::makeBluetoothPanelSpec(state, cfg, actions);
  case 5:
    return ui::panels::makeBluetoothScanPanelSpec(state, cfg, actions);
  case 6:
    return ui::panels::makeSystemPanelSpec(state, actions);
    case 7:
    return ui::panels::makeAboutPanelSpec(state, actions);
  default:
    return {};
  }
}
} // namespace

void drawShellImGui(ShellState &state, Config &cfg,
                    const ShellActions &actions) {
  const ImGuiViewport *vp = ImGui::GetMainViewport();
  float W = vp->WorkSize.x, H = vp->WorkSize.y;
  bool panel_open =
      state.anyPanelOpen() || state.panel_anim != ShellState::PanelAnim::Idle;

  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
  ImGui::SetNextWindowPos(vp->WorkPos);
  ImGui::SetNextWindowSize(vp->WorkSize);
  ImGuiWindowFlags flags =
      ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground |
      ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoScrollbar |
      ImGuiWindowFlags_NoScrollWithMouse;
  ImGui::Begin("##ludex", nullptr, flags);
  ImDrawList *dl = ImGui::GetWindowDrawList();

  // --- Wallpaper ---
  if (state.wallpapers.hasWallpaper()) {
    state.wallpapers.draw(dl, W, H, cfg);
  } else {
    bool light = isLight(cfg.theme);
    dl->AddRectFilled(ImVec2(0, 0), ImVec2(W, H),
                      light ? IM_COL32(245, 245, 245, 255)
                            : IM_COL32(18, 18, 20, 255));
  }

  // --- Componentes principales ---
  g_edge_fades.draw(cfg, W, H);
  g_tile_carousel.draw(state, cfg, actions, W, H, panel_open);

  // --- Paneles (usando el renderer genérico + caché) ---
  {
    int panel_id = state.drawPanelId();
    bool need_rebuild = false;
    int cache_token = 0;

    switch (panel_id) {
    case 0: // sin panel: reseteamos el caché
      g_panel_cache.panel_id = 0;
      g_panel_cache.token = -1;
      break;

    // Paneles estáticos: solo reconstruir al cambiar de panel
    case 1:
    case 2:
    case 6:
      cache_token = 0;
      need_rebuild = (g_panel_cache.panel_id != panel_id);
      break;

    // Controllers: depende de la lista de mandos y del modo picking
    case 3:
      cache_token =
          state.panel_cache_token * 16 + (state.controller_pick_player + 1);
      need_rebuild = (g_panel_cache.panel_id != panel_id ||
                      g_panel_cache.token != cache_token);
      break;

    // Bluetooth: depende de la lista de dispositivos
    case 4:
      cache_token = state.panel_cache_token;
      need_rebuild = (g_panel_cache.panel_id != panel_id ||
                      g_panel_cache.token != cache_token);
      break;

    // Bluetooth Scan: depende del estado de escaneo y del contador
    case 5: {
      int rem = (actions.bluetooth_scan_remaining)
                    ? actions.bluetooth_scan_remaining()
                    : 0;
      bool scanning =
          (actions.bluetooth_scanning) && actions.bluetooth_scanning();
      cache_token =
          state.panel_cache_token * 100 + rem * 2 + (scanning ? 1 : 0);
      need_rebuild = (g_panel_cache.panel_id != panel_id ||
                      g_panel_cache.token != cache_token);
      break;
    }
case 7:  // About: estático
    cache_token = 0;
    need_rebuild = (g_panel_cache.panel_id != panel_id);
    break;    default:
      break;
    }

    if (panel_id != 0) {
      if (need_rebuild) {
        g_panel_cache.panel_id = panel_id;
        g_panel_cache.token = cache_token;
        g_panel_cache.spec = buildPanelSpec(panel_id, state, cfg, actions);
      }
      ui::panels::drawGenericPanel(g_panel_cache.spec, state, cfg, actions);
    }
  }

  // Modal PIN por encima de cualquier panel
  if (state.show_pin) {
    int pin_token = state.panel_cache_token;
    if (g_pin_cache.token != pin_token) {
      g_pin_cache.token = pin_token;
      g_pin_cache.spec = ui::panels::makePinPanelSpec(state, actions);
    }
    ui::panels::drawGenericPanel(g_pin_cache.spec, state, cfg, actions);
  }
  // --- Widgets de HUD ---
  bool horizontal = isHorizontal(cfg.side);
  bool left = (cfg.side == LayoutSide::Left);
  bool top = (cfg.side == LayoutSide::Top);
  bool clock_bottom = horizontal && top;
  g_clock.draw(cfg, vp, left, clock_bottom);

  if (cfg.show_player_indicators) {
    g_player_indicators.draw(state, cfg, actions.player_status(), vp);
  }

  if (!panel_open) {
    g_help_hints.draw(state, cfg, vp);
  }
  state.toasts.draw(dl, W, H, isLight(cfg.theme));
  ImGui::End();
  ImGui::PopStyleVar();
}