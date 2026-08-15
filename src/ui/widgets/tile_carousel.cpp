#include "tile_carousel.h"
#include "../ui_common.h"
#include "imgui_internal.h"
#include <algorithm>
#include <cmath>

namespace ui::widgets {

void TileCarousel::draw(ShellState &state, const Config &cfg,
                        const ShellActions &actions, float W, float H,
                        bool panel_open) {
  ImDrawList *dl = ImGui::GetWindowDrawList();
  bool horizontal = isHorizontal(cfg.side);
  bool left = (cfg.side == LayoutSide::Left);
  bool top = (cfg.side == LayoutSide::Top);
  int V = cfg.visible_items;
  float k = cfg.tile_sel_ratio;
  int N = (int)state.apps.size();

  float main_axis = horizontal ? W : H;
  float cross_axis = horizontal ? H : W;
  float slot = main_axis / ((float)(V - 1) + k);
  float sel_size = slot * k;
  float cross_un = cross_axis * cfg.tile_w_pct;
  float cross_sel = cross_axis * cfg.tile_sel_w_pct;
  float icon_un =
      cross_axis * cfg.icon_pct * (horizontal ? 1.5f : cfg.icon_vert_scale);
  float icon_sel =
      cross_axis * cfg.icon_sel_pct * (horizontal ? 1.5f : cfg.icon_vert_scale);
  float pad = cross_axis * 0.012f;

  auto centerMain = [&](float d) -> float {
    float ad = std::fabs(d);
    float sgn = d >= 0.0f ? 1.0f : -1.0f;
    float s = sgn * smooth(std::min(ad, 1.0f));
    return main_axis * 0.5f + (sel_size - slot) * 0.5f * s + slot * d;
  };

  struct Row {
    int id, idx;
    float d, h, main_pos;
  };
  std::vector<Row> rows;
  const float half = (float)V * 0.5f + 1.0f;
  int row_id = 0;

  for (int i = 0; i < N; ++i) {
    float d0 = wrapHalf((float)i - state.offset, N);
    int mmax = (int)std::ceil(half / (float)N) + 1;
    for (int m = -mmax; m <= mmax; ++m) {
      float d = d0 + (float)m * (float)N;
      if (std::fabs(d) > half)
        continue;
      float t = smooth(std::clamp(1.0f - std::fabs(d), 0.0f, 1.0f));
      rows.push_back(Row{row_id++, i, d, std::lerp(slot, sel_size, t), 0.0f});
    }
  }

  std::sort(rows.begin(), rows.end(),
            [](const Row &a, const Row &b) { return a.d < b.d; });

  if (!rows.empty()) {
    int f = 0;
    for (int j = 0; j < (int)rows.size(); ++j)
      if (std::fabs(rows[j].d) < std::fabs(rows[f].d))
        f = j;
    rows[f].main_pos = centerMain(rows[f].d);
    for (int j = f - 1; j >= 0; --j)
      rows[j].main_pos =
          rows[j + 1].main_pos - (rows[j].h + rows[j + 1].h) * 0.5f;
    for (int j = f + 1; j < (int)rows.size(); ++j)
      rows[j].main_pos =
          rows[j - 1].main_pos + (rows[j - 1].h + rows[j].h) * 0.5f;
  }

  bool accept_tile_input = !state.menu_open && !panel_open;

  for (const Row &row : rows) {
    const App &app = state.apps[row.idx];
    float t = smooth(std::clamp(1.0f - std::fabs(row.d), 0.0f, 1.0f));
    float h_i = row.h;
    float w_i = flerp(cross_un, cross_sel, t);
    float x0, y0;

    if (horizontal) {
      x0 = row.main_pos - h_i * 0.5f;
      y0 = top ? 0.0f : H - w_i;
    } else {
      x0 = left ? 0.0f : W - w_i;
      y0 = row.main_pos - h_i * 0.5f;
    }

    ImVec2 min(x0, y0);
    ImVec2 max(x0 + (horizontal ? h_i : w_i), y0 + (horizontal ? w_i : h_i));

    bool tile_hovered = false;
    bool tile_active = false;

    if (accept_tile_input) {
      ImGui::PushID(row.id);
      ImGui::SetCursorScreenPos(min);
      ImGui::InvisibleButton("##tile", ImVec2(max.x - min.x, max.y - min.y));
      tile_hovered = ImGui::IsItemHovered();
      tile_active = ImGui::IsItemActive();

      if (tile_hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        if (t > 0.9f)
          state.pending_launch = row.idx;
        else
          state.selected = row.idx;
      }
      ImGui::PopID();
    }

    float bright = flerp(0.72f, 1.0f, t);
    if (tile_active)
      bright *= 0.85f;

    if (app.tile_type == 1) {
      ImU32 c1 = colScaled(app.color1, bright);
      ImU32 c2 = colScaled(app.color2, bright);
      dl->AddRectFilledMultiColor(min, max, c1, c2, c2, c1);
    } else {
      dl->AddRectFilled(min, max, colScaled(app.color1, bright));
    }

    if (tile_hovered && !tile_active) {
      bool light = isLight(cfg.theme);
      ImU32 hover_border =
          light ? IM_COL32(100, 120, 200, 180) : IM_COL32(150, 170, 255, 180);
      dl->AddRect(min, max, hover_border, 0.0f, 0, 2.0f);
    }

    std::string label = upper(app.name);
    if (!g_font_tile)
      ImGui::PushFont(ImGui::GetDefaultFont());
    else
      ImGui::PushFont(ui::g_font_tile);

    float fs_sel = H * cfg.font_tile_pct;
    float fs = flerp(fs_sel * 0.78f, fs_sel, t);
    ImVec2 ts = g_font_tile ? g_font_tile->CalcTextSizeA(fs, 10000.0f, 0.0f,
                                                         label.c_str(), nullptr)
                            : ImGui::CalcTextSize(label.c_str());
    float isz = flerp(icon_un, icon_sel, t);

    ImU32 label_col;
    if (app.has_text_color) {
      label_col =
          IM_COL32(app.text_color.r, app.text_color.g, app.text_color.b, 255);
    } else if (app.has_icon_tint) {
      label_col =
          IM_COL32(app.icon_tint.r, app.icon_tint.g, app.icon_tint.b, 255);
    } else {
      label_col = IM_COL32(255, 255, 255, 255);
    }

    if (horizontal) {
      float icon_x = x0 + (h_i - isz) * 0.5f;
      float label_x = x0 + (h_i - ts.x) * 0.5f;
      float icon_y, label_y;
      if (top) {
        label_y = y0 + w_i * 0.05f;
        float area_top = label_y + ts.y;
        icon_y = area_top + ((y0 + w_i) - area_top - isz) * 0.5f;
      } else {
        label_y = y0 + w_i * 0.95f - ts.y;
        icon_y = y0 + (label_y - y0 - isz) * 0.5f;
      }
      if (app.icon_texture) {
        dl->AddImage((ImTextureID)app.icon_texture.get(),
                     ImVec2(icon_x, icon_y),
                     ImVec2(icon_x + isz, icon_y + isz));
      }
      dl->AddText(g_font_tile, fs, ImVec2(label_x, label_y), label_col,
                  label.c_str());
    } else {
      if (app.icon_texture) {
        ImVec2 imin(x0 + pad, row.main_pos - isz * 0.5f);
        dl->AddImage((ImTextureID)app.icon_texture.get(), imin,
                     ImVec2(imin.x + isz, imin.y + isz));
      }
      dl->AddText(g_font_tile, fs,
                  ImVec2(x0 + pad + isz + pad, row.main_pos - ts.y * 0.5f),
                  label_col, label.c_str());
    }
    ImGui::PopFont();
  }
}

} // namespace ui::widgets