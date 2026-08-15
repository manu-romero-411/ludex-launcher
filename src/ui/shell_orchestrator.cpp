#include "shell_orchestrator.h"
#include "panels/pan_bluetooth.h"
#include "panels/pan_bluetooth_scan.h"
#include "panels/pan_controllers.h"
#include "panels/pan_settings.h"
#include "panels/pan_shutdown.h"
#include "panels/panel_renderer.h"
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
} // namespace

void drawShellImGui(ShellState &state, const Config &cfg,
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

  // --- Paneles (usando el renderer genérico) ---
  Config &mutable_cfg = const_cast<Config &>(cfg);
  switch (state.drawPanelId()) {
  case 1: {
    auto s = ui::panels::makeSettingsPanelSpec(state, mutable_cfg, actions);
    ui::panels::drawGenericPanel(s, state, mutable_cfg, actions);
    break;
  }
  case 2: {
    auto s = ui::panels::makeShutdownPanelSpec(state, actions);
    ui::panels::drawGenericPanel(s, state, mutable_cfg, actions);
    break;
  }
  case 3: {
    auto s = ui::panels::makeControllersPanelSpec(state, mutable_cfg, actions);
    ui::panels::drawGenericPanel(s, state, mutable_cfg, actions);
    break;
  }
  case 4: {
    auto s = ui::panels::makeBluetoothPanelSpec(state, mutable_cfg, actions);
    ui::panels::drawGenericPanel(s, state, mutable_cfg, actions);
    break;
  }
  case 5: {
    auto s =
        ui::panels::makeBluetoothScanPanelSpec(state, mutable_cfg, actions);
    ui::panels::drawGenericPanel(s, state, mutable_cfg, actions);
    break;
  }
  case 6: {
    auto s = ui::panels::makeSystemPanelSpec(state, actions);
    ui::panels::drawGenericPanel(s, state, mutable_cfg, actions);
    break;
  }
  default:
    break;
  }

  // Modal PIN por encima de cualquier panel
  if (state.show_pin) {
    auto spec = ui::panels::makePinPanelSpec(state, actions);
    ui::panels::drawGenericPanel(spec, state, mutable_cfg, actions);
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