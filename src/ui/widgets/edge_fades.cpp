#include "edge_fades.h"
#include <algorithm>

namespace ui::widgets {

void EdgeFades::draw(const Config& cfg, float W, float H) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    int a = (int)std::clamp(cfg.edge_fade_alpha, 0.0f, 255.0f);
    bool light = (cfg.theme == "light");
    ImU32 edge = light ? IM_COL32(245, 245, 245, a) : IM_COL32(0, 0, 0, a);
    ImU32 none = light ? IM_COL32(245, 245, 245, 0) : IM_COL32(0, 0, 0, 0);

    float left_fw = W * (cfg.tile_sel_w_pct + cfg.edge_fade_pct);
    dl->AddRectFilledMultiColor(ImVec2(0.0f, 0.0f), ImVec2(left_fw, H), edge, none, none, edge);

    float right_fw = W * cfg.edge_fade_pct;
    dl->AddRectFilledMultiColor(ImVec2(W - right_fw, 0.0f), ImVec2(W, H), none, edge, edge, none);
}

} // namespace ui::widgets