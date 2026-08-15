// src/ui/panels/panel_renderer.cpp (función simplificada)
#include "panel_renderer.h"
#include "../ui_common.h"
#include <algorithm>
#include <imgui.h>

namespace ui::panels {

namespace {

struct Layout {
  ImVec2 panel_min, panel_max;
  ImVec2 content_min, content_max;
  float title_h, row_h, footer_h, pad;
  float pw, ph, px, py;
  ImU32 fade_col, panel_bg, panel_title;
  ImU32 row_focus, row_hover, row_pressed;
  ImU32 text_main, text_val, border_col;

  ImVec2 rowMin(int i) const {
    return ImVec2(content_min.x, content_min.y + i * row_h);
  }
  ImVec2 rowMax(int i) const {
    return ImVec2(content_max.x, content_min.y + (i + 1) * row_h);
  }
};

Layout computeLayout(int list_count, const Config &cfg) {
  Layout L{};
  const ImGuiViewport *vp = ImGui::GetMainViewport();
  float W = vp->WorkSize.x, H = vp->WorkSize.y;
  bool light = (cfg.theme == "light");

  L.fade_col = light ? IM_COL32(240, 240, 240, 210) : IM_COL32(0, 0, 0, 210);
  L.panel_bg = light ? IM_COL32(250, 250, 252, 250) : IM_COL32(30, 32, 40, 250);
  L.panel_title =
      light ? IM_COL32(230, 232, 240, 255) : IM_COL32(44, 47, 60, 255);
  L.row_focus =
      light ? IM_COL32(210, 220, 240, 255) : IM_COL32(78, 82, 98, 255);
  L.row_hover =
      light ? IM_COL32(220, 230, 245, 255) : IM_COL32(58, 62, 78, 255);
  L.row_pressed =
      light ? IM_COL32(190, 200, 220, 255) : IM_COL32(98, 102, 118, 255);
  L.text_main =
      light ? IM_COL32(25, 25, 30, 255) : IM_COL32(230, 230, 230, 255);
  L.text_val = light ? IM_COL32(40, 40, 50, 255) : IM_COL32(240, 240, 240, 255);
  L.border_col =
      light ? IM_COL32(120, 125, 140, 255) : IM_COL32(150, 155, 170, 200);

  L.row_h = H * 0.055f;
  L.title_h = H * 0.09f;
  L.footer_h = L.row_h * 1.6f;
  L.pad = W * 0.012f;
  L.pw = W * 0.56f;
  L.ph = L.title_h + list_count * L.row_h + L.footer_h;
  L.px = (W - L.pw) * 0.5f;
  L.py = (H - L.ph) * 0.5f;

  L.panel_min = ImVec2(L.px, L.py);
  L.panel_max = ImVec2(L.px + L.pw, L.py + L.ph);
  L.content_min = ImVec2(L.px, L.py + L.title_h);
  L.content_max = ImVec2(L.px + L.pw, L.py + L.title_h + list_count * L.row_h);
  return L;
}

} // anonymous namespace

void drawGenericPanel(const PanelSpec &spec, ShellState &st, Config &cfg,
                      const ShellActions &actions) {
  if (spec.rows.empty())
    return;

  const int count = (int)spec.rows.size();
  int &focus = *spec.focus_ptr;
  focus = std::clamp(focus, 0, count - 1);

  // --- LAYOUT + CHROME ---
  Layout L = computeLayout(count, cfg);
  const ImGuiViewport *vp = ImGui::GetMainViewport();
  ImDrawList *dl = ImGui::GetWindowDrawList();

  dl->AddRectFilled(ImVec2(0, 0), ImVec2(vp->WorkSize.x, vp->WorkSize.y),
                    L.fade_col);
  dl->AddRectFilled(L.panel_min, L.panel_max, L.panel_bg, 10.0f);
  dl->AddRectFilled(L.panel_min, ImVec2(L.panel_max.x, L.py + L.title_h),
                    L.panel_title, 10.0f, ImDrawFlags_RoundCornersTop);

  ImGui::PushFont(ui::g_font_tile);
  ImVec2 tt = ImGui::CalcTextSize(spec.title.c_str());
  dl->AddText(
      ImVec2(L.px + (L.pw - tt.x) * 0.5f, L.py + (L.title_h - tt.y) * 0.5f),
      L.text_main, spec.title.c_str());
  ImGui::PopFont();

  // --- FILAS ---
  ImGui::PushFont(ui::g_font_tile);
  ImVec2 txt_h = ImGui::CalcTextSize("X");

  for (int i = 0; i < count; ++i) {
    const auto &row = spec.rows[i];
    ImVec2 rmin = L.rowMin(i);
    ImVec2 rmax = L.rowMax(i);

    ImGui::PushID(9000 + i);
    ImGui::SetCursorScreenPos(rmin);
    ImGui::PushID(9000 + i);
    ImGui::SetCursorScreenPos(rmin);
    ImGui::InvisibleButton("##row", ImVec2(rmax.x - rmin.x, rmax.y - rmin.y));
    bool hovered = ImGui::IsItemHovered();
    bool active = ImGui::IsItemActive();

    if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
      focus = i;
      if (row.on_select)
        row.on_select(st, cfg, actions);
      else if (row.adjust)
        row.adjust(cfg, +1);
    }
    ImGui::PopID();

    if (active)
      dl->AddRectFilled(rmin, rmax, L.row_pressed);
    else if (i == focus)
      dl->AddRectFilled(rmin, rmax, L.row_focus);
    else if (hovered)
      dl->AddRectFilled(rmin, rmax, L.row_hover);

    // Icono + label
    void *ricon = st.ui_icons.byIndex(row.icon);
    float isz = L.row_h * 0.6f;
    float lx = L.content_min.x + L.pad;
    if (ricon) {
      dl->AddImage((ImTextureID)ricon,
                   ImVec2(lx, rmin.y + (L.row_h - isz) * 0.5f),
                   ImVec2(lx + isz, rmin.y + (L.row_h + isz) * 0.5f),
                   ImVec2(0, 0), ImVec2(1, 1), L.text_main);
      lx += isz + L.pad * 0.8f;
    }
    ImVec2 lt = ImGui::CalcTextSize(row.label.c_str());
    dl->AddText(ImVec2(lx, rmin.y + (L.row_h - txt_h.y) * 0.5f), L.text_main,
                row.label.c_str());

    // Valor a la derecha
    if (row.get_value) {
      std::string val = row.get_value(cfg);
      if (row.adjust)
        val = "<  " + val + "  >";
      ImVec2 vs = ImGui::CalcTextSize(val.c_str());
      dl->AddText(ImVec2(L.content_max.x - L.pad - vs.x,
                         rmin.y + (L.row_h - vs.y) * 0.5f),
                  L.text_val, val.c_str());
    }
  }
  ImGui::PopFont();
}

} // namespace ui::panels