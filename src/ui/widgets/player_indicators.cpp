#include "player_indicators.h"
#include <algorithm>

namespace ui::widgets {

void PlayerIndicators::draw(
    const ShellState &state, const Config &cfg,
    const std::vector<InputManager::PlayerStatus> &players,
    const ImGuiViewport *vp) {
  if (players.empty())
    return;

  ImDrawList *dl = ImGui::GetWindowDrawList();
  float W = vp->WorkSize.x, H = vp->WorkSize.y;
  bool light = isLight(cfg.theme);
  ImU32 active = IM_COL32(0, 255, 90, 255);

  bool gp_top = (cfg.side == LayoutSide::Bottom);
  bool gp_right = (cfg.side != LayoutSide::Right);
  ImU32 idle = light ? IM_COL32(40, 40, 45, 220) : IM_COL32(220, 222, 228, 220);
  float s = H * 0.024f;
  float gap = s * 0.4f;
  float margin = H * 0.018f;
  int n = std::min((int)players.size(), 8);
  float total = n * s + (n - 1) * gap;
  float x_start = gp_right ? (W - margin - total) : margin;
  float y = gp_top ? margin : (H - margin - s);

  for (int i = 0; i < n; ++i) {
    ImU32 col = players[i].active ? active : idle;
    float x = x_start + i * (s + gap);

    if (state.ui_icons.gamepad) {
      dl->AddImage((ImTextureID)state.ui_icons.gamepad, ImVec2(x, y),
                   ImVec2(x + s, y + s), ImVec2(0, 0), ImVec2(1, 1), col);
    } else {
      drawGamepadGlyph(dl, ImVec2(x, y), ImVec2(x + s, y + s), col);
    }
  }
}

void PlayerIndicators::drawGamepadGlyph(ImDrawList *dl, ImVec2 min, ImVec2 max,
                                        ImU32 col) {
  float w = max.x - min.x, h = max.y - min.y;
  ImVec2 bmin(min.x, min.y + h * 0.22f);
  ImVec2 bmax(max.x, max.y - h * 0.10f);
  dl->AddRectFilled(bmin, bmax, col, h * 0.28f);
  dl->AddCircleFilled(ImVec2(min.x + w * 0.20f, min.y + h * 0.28f), h * 0.16f,
                      col);
  dl->AddCircleFilled(ImVec2(max.x - w * 0.20f, min.y + h * 0.28f), h * 0.16f,
                      col);
}

} // namespace ui::widgets