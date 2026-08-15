#include "system_menu.h"
#include "../ui_common.h"
#include <imgui.h>

namespace ui::widgets {

namespace {
constexpr int SYSTEM_MENU_BASE = 500;
}

void SystemMenu::draw(ShellState &state, const Config &cfg,
                      const ShellActions &actions, float W, float H) {
  if (state.menu_anim <= 0.001f)
    return;

  ImDrawList *dl = ImGui::GetWindowDrawList();
  bool left =
      (cfg.side == LayoutSide::Left) || (cfg.side == LayoutSide::Bottom);
  bool light = isLight(cfg.theme);
  ImU32 row_normal =
      light ? IM_COL32(230, 232, 240, 255) : IM_COL32(32, 32, 36, 255);
  ImU32 row_focus =
      light ? IM_COL32(200, 210, 230, 255) : IM_COL32(74, 74, 80, 255);
  ImU32 text_col =
      light ? IM_COL32(25, 25, 30, 255) : IM_COL32(230, 230, 230, 255);

  float menu_h = H * cfg.menu_h_pct;
  float menu_w = W * cfg.tile_sel_w_pct;
  float total = menu_h * 4.0f;
  float y_base = H - total * state.menu_anim;

  static const char *labels[5] = {"SETTINGS", "BLUETOOTH", "CONTROLLERS",
                                  "QUIT TO DESKTOP", "SHUTDOWN..."};
  void *icons[5] = {state.ui_icons.settings.get(),
                    state.ui_icons.gamepad.get(), // TODO: icono bluetooth
                    state.ui_icons.gamepad.get(), state.ui_icons.exit.get(),
                    state.ui_icons.shutdown.get()};
  for (int m = 0; m < 4; ++m) {
    float y0 = y_base + m * menu_h;
    ImVec2 min(left ? 0.0f : W - menu_w, y0);
    ImVec2 max(min.x + menu_w, y0 + menu_h);

    if (state.menu_open) {
      ImGui::PushID(SYSTEM_MENU_BASE + m);
      ImGui::SetCursorScreenPos(min);
      ImGui::InvisibleButton("##sys", ImVec2(menu_w, menu_h));
      bool hovered = ImGui::IsItemHovered();
      bool active = ImGui::IsItemActive();

      if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        state.menu_selected = m;
        if (m == 0) {
          actions.open_settings();
        } else if (m == 1 && actions.open_bluetooth) {
          actions.open_bluetooth();
        } else if (m == 2 && actions.open_controllers) {
          actions.open_controllers();
        } else if (m == 3) {
          actions.quit();
        } else {
          state.show_power = true;
          state.menu_open = false;
          state.power_focus = 0;
        }
      }
      ImGui::PopID();

      ImU32 col;
      if (active) {
        col = light ? IM_COL32(180, 190, 210, 255) : IM_COL32(94, 94, 100, 255);
      } else if (hovered) {
        col = light ? IM_COL32(215, 225, 235, 255) : IM_COL32(54, 54, 60, 255);
      } else if (state.menu_selected == m) {
        col = row_focus;
      } else {
        col = row_normal;
      }
      dl->AddRectFilled(min, max, col);
    } else {
      ImU32 col = (state.menu_selected == m) ? row_focus : row_normal;
      dl->AddRectFilled(min, max, col);
    }

    std::string label = labels[m];
    ImGui::PushFont(g_font_tile);
    ImVec2 ts = ImGui::CalcTextSize(label.c_str());
    float isz = menu_h * 0.55f;
    float lx = min.x + W * 0.012f;

    if (icons[m]) {
      dl->AddImage((ImTextureID)icons[m],
                   ImVec2(lx, y0 + (menu_h - isz) * 0.5f),
                   ImVec2(lx + isz, y0 + (menu_h + isz) * 0.5f), ImVec2(0, 0),
                   ImVec2(1, 1), text_col);
      lx += isz + W * 0.010f;
    }
    dl->AddText(ImVec2(lx, y0 + (menu_h - ts.y) * 0.5f), text_col,
                label.c_str());
    ImGui::PopFont();
  }
}

} // namespace ui::widgets