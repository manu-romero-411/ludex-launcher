#include "panel_renderer.h"
#include "../../core/config.h"
#include "../ui_common.h"
#include <algorithm>
#include <imgui.h>

namespace ui::panels {

namespace {

constexpr int PANEL_ID_BASE = 9000;
constexpr int FOOTER_ID_BASE = 9500;
constexpr int SCROLLBAR_ID = 9600;

struct Layout {
  ImVec2 panel_min, panel_max;
  ImVec2 content_min, content_max; // área scrollable de filas
  float title_h, row_h, footer_h, pad;
  float pw, ph, px, py;
  int visible_rows = 0;
  bool scrollable = false;
  ImU32 fade_col, panel_bg, panel_title;
  ImU32 row_focus, row_hover, row_pressed;
  ImU32 text_main, text_val, border_col;
  ImU32 scroll_track, scroll_thumb;
};

Layout computeLayout(int list_count, const Config &cfg) {
  Layout L{};
  const ImGuiViewport *vp = ImGui::GetMainViewport();
  float W = vp->WorkSize.x, H = vp->WorkSize.y;
  bool light = isLight(cfg.theme);

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
  L.scroll_track =
      light ? IM_COL32(180, 185, 195, 120) : IM_COL32(70, 74, 88, 120);
  L.scroll_thumb =
      light ? IM_COL32(120, 125, 140, 220) : IM_COL32(140, 145, 160, 220);

  L.row_h = H * 0.055f;
  L.title_h = H * 0.09f;
  L.footer_h = L.row_h * 1.6f;
  L.pad = W * 0.012f;
  L.pw = W * 0.56f;

  // El panel no puede pasar del 80% de la pantalla:
  // si hay más filas, se recortan y aparece el scroll.
  float max_panel_h = H * 0.80f;
  int max_rows =
      (int)std::floor((max_panel_h - L.title_h - L.footer_h) / L.row_h);
  L.visible_rows = std::max(1, std::min(list_count, std::max(1, max_rows)));
  L.scrollable = list_count > L.visible_rows;

  float content_h = (float)L.visible_rows * L.row_h;
  L.ph = L.title_h + content_h + L.footer_h;
  L.px = (W - L.pw) * 0.5f;
  L.py = (H - L.ph) * 0.5f;

  L.panel_min = ImVec2(L.px, L.py);
  L.panel_max = ImVec2(L.px + L.pw, L.py + L.ph);
  L.content_min = ImVec2(L.px, L.py + L.title_h);
  L.content_max = ImVec2(L.px + L.pw, L.py + L.title_h + content_h);
  return L;
}

// Rectángulo centrado para el botón f del footer (de n totales)
void footerRect(const Layout &L, int f, int n, ImVec2 &out_min,
                ImVec2 &out_max) {
  float bw = L.pw * 0.24f;
  float bh = L.row_h * 0.95f;
  float gap = L.pw * 0.04f;
  float total = n * bw + (n - 1) * gap;
  float x0 = L.px + (L.pw - total) * 0.5f + f * (bw + gap);
  float y0 = L.content_max.y + (L.footer_h - bh) * 0.5f;
  out_min = ImVec2(x0, y0);
  out_max = ImVec2(x0 + bw, y0 + bh);
}

} // namespace

void drawGenericPanel(const PanelSpec &spec, ShellState &st, Config &cfg,
                      const ShellActions &actions) {
  if (spec.rows.empty())
    return;

  // Separar filas de lista / filas de footer (Footer al final)
  int total = (int)spec.rows.size();
  int footer_count = 0;
  for (int i = total - 1; i >= 0 && spec.rows[i].kind == RowKind::Footer; --i)
    ++footer_count;
  int list_count = total - footer_count;

  int &focus = *spec.focus_ptr;
  const bool accept_mouse = !st.show_pin || spec.pin_panel;

  Layout L = computeLayout(list_count, cfg);

  // ---- animación de deslizamiento + fundido del fondo ----
  const ImGuiViewport *vpa = ImGui::GetMainViewport();
  bool animating = st.panel_anim != ShellState::PanelAnim::Idle;
  float eased = animating ? st.panelEased() : 1.0f;

  // Recorrido completo: desde el reposo hasta quedar totalmente bajo la
  // pantalla
  float hide_dist = vpa->WorkSize.y - L.py;
  float anim_offset = (1.0f - eased) * hide_dist;

  L.py += anim_offset;
  L.panel_min.y += anim_offset;
  L.panel_max.y += anim_offset;
  L.content_min.y += anim_offset;
  L.content_max.y += anim_offset;

  // El velo de fondo aparece/desaparece con el panel (ya gradual)
  if (animating) {
    int a = (int)(210 * eased);
    L.fade_col =
        isLight(cfg.theme) ? IM_COL32(240, 240, 240, a) : IM_COL32(0, 0, 0, a);
  }

  // ---- estado de scroll ----
  float scroll = spec.scroll_ptr ? *spec.scroll_ptr : 0.0f;
  float max_scroll = std::max(0, list_count - L.visible_rows);
  // auto-seguir al foco (solo filas de lista; el footer es fijo)
  if (focus < list_count) {
    if ((float)focus < scroll)
      scroll = (float)focus;
    if ((float)focus > scroll + L.visible_rows - 1)
      scroll = (float)focus - L.visible_rows + 1;
  }
  scroll = std::clamp(scroll, 0.0f, max_scroll);

  const ImGuiViewport *vp = ImGui::GetMainViewport();
  ImDrawList *dl = ImGui::GetWindowDrawList();
  ImGuiIO &io = ImGui::GetIO();

  // ---- chrome ----
  dl->AddRectFilled(ImVec2(0, 0), ImVec2(vp->WorkSize.x, vp->WorkSize.y),
                    L.fade_col);
  dl->AddRectFilled(L.panel_min, L.panel_max, L.panel_bg, 10.0f);
  dl->AddRectFilled(L.panel_min, ImVec2(L.panel_max.x, L.py + L.title_h),
                    L.panel_title, 10.0f, ImDrawFlags_RoundCornersTop);

  ImGui::PushFont(ui::g_font_panel_title);
  ImVec2 tt = ImGui::CalcTextSize(spec.title.c_str());
  dl->AddText(
      ui::g_font_panel_title, ui::g_font_panel_title->FontSize,
      ImVec2(L.px + (L.pw - tt.x) * 0.5f, L.py + (L.title_h - tt.y) * 0.5f),
      L.text_main, spec.title.c_str());
  ImGui::PopFont();

  // rueda del ratón sobre el área de contenido
  if (L.scrollable && io.MouseWheel != 0.0f &&
      ImGui::IsMouseHoveringRect(L.content_min, L.content_max)) {
    scroll = std::clamp(scroll - io.MouseWheel * 2.0f, 0.0f, max_scroll);
  }

  // ---- filas (scrollables, recortadas) ----
  // ---- filas (scrollables, recortadas) ----
  int first = (int)std::floor(scroll);
  int last = std::min(list_count - 1, first + L.visible_rows);

  dl->PushClipRect(L.content_min, L.content_max, true);
  for (int i = first; i <= last; ++i) {
    const auto &row = spec.rows[i];
    float y0 = L.content_min.y + ((float)i - scroll) * L.row_h;
    ImVec2 rmin(L.content_min.x, y0);
    ImVec2 rmax(L.content_max.x, y0 + L.row_h);

    ImGui::PushID(PANEL_ID_BASE + i); // <-- CORREGIDO
    ImGui::SetCursorScreenPos(rmin);
    ImGui::InvisibleButton(
        "##row", ImVec2(rmax.x - rmin.x, rmax.y - rmin.y)); // <-- CORREGIDO

    bool hovered = false, active = false;
    if (accept_mouse) {
      hovered = ImGui::IsItemHovered();
      active = ImGui::IsItemActive();
      if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        focus = i;
        if (row.on_select)
          row.on_select(st, cfg, actions);
        else if (row.adjust)
          row.adjust(cfg, +1);
      }
    }
    ImGui::PopID();

    if (active)
      dl->AddRectFilled(rmin, rmax, L.row_pressed);
    else if (i == focus)
      dl->AddRectFilled(rmin, rmax, L.row_focus);
    else if (hovered)
      dl->AddRectFilled(rmin, rmax, L.row_hover);

    void *ricon = st.ui_icons.byIndex(row.icon);
    float isz = L.row_h * 0.6f;
    float lx = L.content_min.x + L.pad;
    if (ricon) {
      ImU32 icol = row.icon_col ? row.icon_col : L.text_main;
      dl->AddImage((ImTextureID)ricon, ImVec2(lx, y0 + (L.row_h - isz) * 0.5f),
                   ImVec2(lx + isz, y0 + (L.row_h + isz) * 0.5f), ImVec2(0, 0),
                   ImVec2(1, 1), icol);
      lx += isz + L.pad * 0.8f;
    }
    ImGui::PushFont(ui::g_font_panel_row);
    ImVec2 lt = ImGui::CalcTextSize(row.label.c_str());
    dl->AddText(ui::g_font_panel_row, ui::g_font_panel_row->FontSize,
                ImVec2(lx, y0 + (L.row_h - lt.y) * 0.5f), L.text_main,
                row.label.c_str());
    if (row.get_value) {
      std::string val = row.get_value(cfg);
      if (row.adjust)
        val = "<  " + val + "  >";

      // Si el valor no cabe entre el label y el borde derecho,
      // se trunca con "..." (respetando codepoints UTF-8).
      const float gap = L.pad * 2.0f;
      const float max_w = (L.content_max.x - L.pad) - (lx + lt.x + gap);
      if (max_w > 0.0f && ImGui::CalcTextSize(val.c_str()).x > max_w) {
        auto pop_utf8 = [](std::string &s) {
          if (s.empty())
            return;
          size_t i = s.size();
          do {
            --i;
          } while (i > 0 && ((unsigned char)s[i] & 0xC0) == 0x80);
          s.resize(i);
        };
        while (!val.empty() &&
               ImGui::CalcTextSize((val + "...").c_str()).x > max_w)
          pop_utf8(val);
        val += "...";
      }

      ImVec2 vs = ImGui::CalcTextSize(val.c_str());
      dl->AddText(
          ui::g_font_panel_row, ui::g_font_panel_row->FontSize,
          ImVec2(L.content_max.x - L.pad - vs.x, y0 + (L.row_h - vs.y) * 0.5f),
          L.text_val, val.c_str());
    }
    ImGui::PopFont();
  }
  dl->PopClipRect();

  // ---- scrollbar ----
  if (L.scrollable) {
    float track_w = 4.0f;
    float tx = L.content_max.x - L.pad * 0.5f - track_w;
    ImVec2 tmin(tx, L.content_min.y + 4.0f);
    ImVec2 tmax(tx + track_w, L.content_max.y - 4.0f);
    dl->AddRectFilled(tmin, tmax, L.scroll_track, 2.0f);

    float track_h = tmax.y - tmin.y;
    float thumb_h =
        std::max(24.0f, track_h * ((float)L.visible_rows / (float)list_count));
    float thumb_y = tmin.y + (scroll / max_scroll) * (track_h - thumb_h);
    ImVec2 thmin(tx - 2.0f, thumb_y);
    ImVec2 thmax(tx + track_w + 2.0f, thumb_y + thumb_h);

    ImGui::PushID(SCROLLBAR_ID);
    ImGui::SetCursorScreenPos(thmin);
    ImGui::InvisibleButton("##sb",
                           ImVec2(thmax.x - thmin.x, thmax.y - thmin.y));
    bool sb_active = ImGui::IsItemActive();
    bool sb_hover = ImGui::IsItemHovered();
    if (sb_active) {
      float rel = (io.MousePos.y - tmin.y - thumb_h * 0.5f) /
                  std::max(1.0f, track_h - thumb_h);
      scroll = std::clamp(rel * max_scroll, 0.0f, max_scroll);
    }
    ImGui::PopID();
    dl->AddRectFilled(thmin, thmax,
                      (sb_active || sb_hover) ? L.text_val : L.scroll_thumb,
                      3.0f);
  }

  // ---- footer fijo, centrado, siempre visible ----
  for (int f = 0; f < footer_count; ++f) {
    int i = list_count + f;
    const auto &row = spec.rows[i];
    ImVec2 bmin, bmax;
    footerRect(L, f, footer_count, bmin, bmax);

    ImGui::PushID(FOOTER_ID_BASE + i);
    ImGui::SetCursorScreenPos(bmin);
    ImGui::InvisibleButton("##foot", ImVec2(bmax.x - bmin.x, bmax.y - bmin.y));

    bool hovered = false, active = false;
    if (accept_mouse) {
      // <-- CORREGIDO: Se eliminó el PushID e InvisibleButton duplicados que
      // rompían el clic
      hovered = ImGui::IsItemHovered();
      active = ImGui::IsItemActive();
      if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        focus = i;
        if (row.on_select)
          row.on_select(st, cfg, actions);
      }
    }
    ImGui::PopID(); // <-- Ahora cierra correctamente el PushID del principio

    float bh = bmax.y - bmin.y;
    if (active)
      dl->AddRectFilled(bmin, bmax, L.row_pressed, 6.0f);
    else if (focus == i)
      dl->AddRectFilled(bmin, bmax, L.row_focus, 6.0f);
    else if (hovered)
      dl->AddRectFilled(bmin, bmax, L.row_hover, 6.0f);
    dl->AddRect(bmin, bmax, L.border_col, 6.0f, 0, 2.0f);

    void *ricon = st.ui_icons.byIndex(row.icon);
    ImGui::PushFont(ui::g_font_tile);
    ImVec2 lt = ImGui::CalcTextSize(row.label.c_str());
    ImGui::PopFont();
    float isz = bh * 0.5f;
    float gap = isz * 0.35f;
    float total_w = lt.x + (ricon ? isz + gap : 0.0f);
    float x = bmin.x + ((bmax.x - bmin.x) - total_w) * 0.5f;
    if (ricon) {
      dl->AddImage((ImTextureID)ricon, ImVec2(x, bmin.y + (bh - isz) * 0.5f),
                   ImVec2(x + isz, bmin.y + (bh + isz) * 0.5f), ImVec2(0, 0),
                   ImVec2(1, 1), L.text_main);
      x += isz + gap;
    }

    ImGui::PushFont(ui::g_font_tile);
    ImVec2 lt2 = ImGui::CalcTextSize(row.label.c_str());
    dl->AddText(ui::g_font_tile, ui::g_font_tile->FontSize,
                ImVec2(x, bmin.y + (bh - lt2.y) * 0.5f), L.text_main,
                row.label.c_str());
    ImGui::PopFont();
  }

  if (spec.scroll_ptr)
    *spec.scroll_ptr = scroll;
}

void handlePanelAction(const PanelSpec &spec, ShellState &st, Config &cfg,
                       const ShellActions &actions, UiAction a) {
  if (spec.rows.empty() || !spec.focus_ptr)
    return;
  const int count = (int)spec.rows.size();
  int &focus = *spec.focus_ptr;
  focus = std::clamp(focus, 0, count - 1);
  const auto &row = spec.rows[focus];
  switch (a) {
  case UiAction::Up:
    focus = (focus - 1 + count) % count; // wrap: arriba desde 0 -> footer
    break;
  case UiAction::Down:
    focus = (focus + 1) % count; // wrap: abajo desde footer -> 0
    break;
  case UiAction::Left:
    if (row.adjust)
      row.adjust(cfg, -1);
    break;
  case UiAction::Right:
    if (row.adjust)
      row.adjust(cfg, +1);
    break;
  case UiAction::Select:
    if (row.on_select)
      row.on_select(st, cfg, actions);
    else if (row.adjust)
      row.adjust(cfg, +1);
    break;
  case UiAction::Back:
  case UiAction::Guide:
    if (spec.on_back)
      spec.on_back(st, cfg);
    break;
  default:
    break;
  }
}

} // namespace ui::panels