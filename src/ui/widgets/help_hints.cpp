#include "help_hints.h"
#include "../../core/i18n.h"
#include "../ui_common.h"
#include <algorithm>

namespace ui::widgets {

void HelpHints::draw(const ShellState &state, const Config &cfg,
                     const ImGuiViewport *vp) {
  float W = vp->WorkSize.x, H = vp->WorkSize.y;
  ImDrawList *dl = ImGui::GetWindowDrawList();
  bool light = isLight(cfg.theme);
  ImU32 col = light ? IM_COL32(40, 40, 45, 220) : IM_COL32(235, 235, 235, 220);
  const UiIcons &ic = state.ui_icons;

  struct Seg {
    void *tex;
    const char *fallback;
    const char *label;
  };

  bool horiz = isHorizontal(cfg.side);
  std::vector<Seg> segs;

  if (state.show_settings) {
    segs = {{ic.nav_v.get(), _("UP/DOWN"), _("NAVIGATE")},
            {ic.nav_h.get(), _("LEFT/RIGHT"), _("CHANGE")},
            {ic.back.get(), _("B"), _("BACK")}};
  } else if (state.show_controllers) {
    if (state.controller_pick_player >= 0) {
      segs = {{ic.nav_v.get(), _("UP/DOWN"), _("NAVIGATE")},
              {ic.accept.get(), _("A"), _("ASSIGN")},
              {ic.back.get(), _("B"), _("CANCEL")}};
    } else {
      segs = {{ic.nav_v.get(), _("UP/DOWN"), _("NAVIGATE")},
              {ic.accept.get(), _("A"), _("SELECT")},
              {ic.back.get(), _("B"), _("BACK")}};
    }
  } else if (state.show_power) {
    segs = {{ic.nav_v.get(), _("UP/DOWN"), _("NAVIGATE")},
            {ic.accept.get(), _("A"), _("ACCEPT")},
            {ic.back.get(), _("B"), _("BACK")}};
  } else if (state.show_bluetooth_scan) {
    segs = {{ic.nav_v.get(), _("UP/DOWN"), _("NAVIGATE")},
            {ic.accept.get(), _("A"), _("PAIR")},
            {ic.back.get(), _("B"), _("BACK")}};
  } else if (state.show_bluetooth) {
    segs = {{ic.nav_v.get(), _("UP/DOWN"), _("NAVIGATE")},
            {ic.accept.get(), _("A"), _("CONNECT")},
            {nullptr, "X", _("UNLINK")},
            {ic.back.get(), _("B"), _("BACK")}};
  } else if (state.show_system) {
    segs = {{ic.nav_v.get(), _("UP/DOWN"), _("NAVIGATE")},
            {ic.accept.get(), _("A"), _("SELECT")},
            {ic.back.get(), _("B"), _("BACK")}};
  } else {
    segs = {{horiz ? ic.nav_h.get() : ic.nav_v.get(),
             horiz ? "LEFT/RIGHT" : _("UP/DOWN"), _("NAVIGATE")},
            {ic.accept.get(), _("A"), _("SELECT")},
            {ic.home.get(), _("HOME"), _("SYSTEM MENU")}};
  }

  ImGui::PushFont(ui::g_font_hint);
  float gap_seg = W * 0.014f;
  float gap_it = W * 0.004f;

  struct M {
    std::string text;
    float tw, iw;
    ImVec2 ts;
  };

  std::vector<M> ms;
  float total = 0, base_h = 0;
  for (auto &s : segs) {
    M m;
    m.text = s.tex ? s.label : (std::string(s.fallback) + " " + s.label);
    m.ts = ImGui::CalcTextSize(m.text.c_str());
    m.tw = m.ts.x;
    m.iw = s.tex ? m.ts.y * 1.25f : 0.0f;
    base_h = std::max(base_h, m.ts.y);
    total += m.tw + (s.tex ? m.iw + gap_it : 0.0f);
    ms.push_back(m);
  }
  total += gap_seg * (float)(segs.size() - 1);

  float margin = H * 0.008f;
  bool hints_top = (cfg.side == LayoutSide::Bottom);
  float y = hints_top ? margin : (H - margin - base_h);
  float x = (W - total) * 0.5f;

  for (size_t i = 0; i < segs.size(); ++i) {
    const auto &s = segs[i];
    const auto &m = ms[i];
    float cx = x;
    if (s.tex) {
      float hs = base_h * 1.25f;
      float iy = y + (base_h - hs) * 0.5f;
      dl->AddImage((ImTextureID)s.tex, ImVec2(cx, iy), ImVec2(cx + hs, iy + hs),
                   ImVec2(0, 0), ImVec2(1, 1), IM_COL32(255, 255, 255, 220));
      cx += hs + gap_it;
    }
    dl->AddText(ImVec2(cx, y), col, m.text.c_str());
    x += m.tw + (s.tex ? m.iw + gap_it : 0.0f) + gap_seg;
  }
  ImGui::PopFont();
}

} // namespace ui::widgets