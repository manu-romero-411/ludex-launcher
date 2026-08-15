#include "shell_orchestrator.h"
#include "panels/pan_controllers.h"
#include "panels/pan_settings.h"
#include "panels/pan_shutdown.h"
#include "panels/panel_renderer.h"
#include "widgets/clock.h"
#include "widgets/edge_fades.h"
#include "widgets/help_hints.h"
#include "widgets/player_indicators.h"
#include "widgets/system_menu.h"
#include "widgets/tile_carousel.h"
#include "widgets/wallpaper.h"
#include <imgui.h>

namespace {
ui::widgets::Clock g_clock;
ui::widgets::EdgeFades g_edge_fades;
ui::widgets::HelpHints g_help_hints;
ui::widgets::PlayerIndicators g_player_indicators;
ui::widgets::TileCarousel g_tile_carousel;
ui::widgets::SystemMenu g_system_menu; // <-- NUEVO
} // namespace

void drawShellImGui(ShellState &state, const Config &cfg,
                    const ShellActions &actions) {
  const ImGuiViewport *vp = ImGui::GetMainViewport();
  float W = vp->WorkSize.x, H = vp->WorkSize.y;
  bool panel_open =
      state.show_settings || state.show_power || state.show_controllers;

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
  bool has_wp = !state.wallpapers.empty() && state.wp_current >= 0;
  if (has_wp) {
    bool light = (cfg.theme == "light");
    ImU32 overlay_dark = IM_COL32(0, 0, 0, 90);
    ImU32 overlay_light = IM_COL32(255, 255, 255, 80);
    ImU32 overlay = light ? overlay_light : overlay_dark;

    if (state.wp_in_transition && state.wp_next >= 0) {
      float raw = state.wp_fade;
      float f = raw * raw * (3.0f - 2.0f * raw);
      int a_cur = (int)(255 * f);
      int a_nxt = (int)(255 * (1.0f - f));
      ImU32 tint_cur = IM_COL32(255, 255, 255, a_cur);
      ImU32 tint_nxt = IM_COL32(255, 255, 255, a_nxt);
      ui::widgets::drawWallpaperLayer(
          dl, W, H, state.wallpapers[state.wp_current], tint_cur);
      ui::widgets::drawWallpaperLayer(dl, W, H, state.wallpapers[state.wp_next],
                                      tint_nxt);
    } else {
      ui::widgets::drawWallpaperLayer(dl, W, H,
                                      state.wallpapers[state.wp_current],
                                      IM_COL32(255, 255, 255, 255));
    }
    dl->AddRectFilled(ImVec2(0, 0), ImVec2(W, H), overlay);
  } else {
    bool light = (cfg.theme == "light");
    dl->AddRectFilled(ImVec2(0, 0), ImVec2(W, H),
                      light ? IM_COL32(245, 245, 245, 255)
                            : IM_COL32(18, 18, 20, 255));
  }

  // --- Componentes principales ---
  g_edge_fades.draw(cfg, W, H);
  g_tile_carousel.draw(state, cfg, actions, W, H, panel_open);

  // --- Paneles (usando el renderer genérico) ---
  Config &mutable_cfg = const_cast<Config &>(cfg);
  if (state.show_settings) {
    auto spec = ui::panels::makeSettingsPanelSpec(state, mutable_cfg, actions);
    ui::panels::drawGenericPanel(spec, state, mutable_cfg, actions);
  } else if (state.show_power) {
    auto spec = ui::panels::makeShutdownPanelSpec(state, actions);
    ui::panels::drawGenericPanel(spec, state, mutable_cfg, actions);
  } else if (state.show_controllers) {
    auto spec =
        ui::panels::makeControllersPanelSpec(state, mutable_cfg, actions);
    ui::panels::drawGenericPanel(spec, state, mutable_cfg, actions);
  } else {
    if (state.menu_anim > 0.001f) {
      dl->AddRectFilled(ImVec2(0, 0), ImVec2(W, H),
                        IM_COL32(0, 0, 0, (int)(130 * state.menu_anim)));
    }
    g_system_menu.draw(state, cfg, actions, W, H);
  }

  // --- Widgets de HUD ---
  bool horizontal = (cfg.side == "top" || cfg.side == "bottom");
  bool left = (cfg.side == "left");
  bool top = (cfg.side == "top");
  bool clock_bottom = horizontal && top;
  g_clock.draw(cfg, vp, left, clock_bottom);

  if (cfg.show_player_indicators) {
    g_player_indicators.draw(state, cfg, actions.player_status(), vp);
  }

  if (!panel_open) {
    g_help_hints.draw(state, cfg, vp);
  }

  ImGui::End();
  ImGui::PopStyleVar();
}