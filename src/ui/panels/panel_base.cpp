#include "panel_base.h"
#include "../ui_common.h"

namespace ui::panels {

ImVec2 PanelLayout::rowMin(int i) const {
    float y0 = content_min.y + i * row_h;
    return ImVec2(content_min.x, y0);
}

ImVec2 PanelLayout::rowMax(int i) const {
    float y0 = content_min.y + i * row_h;
    return ImVec2(content_max.x, y0 + row_h);
}

ImVec2 PanelLayout::footerButtonMin(int f, int footer_rows, int list_count) const {
    float bh = row_h * 0.95f;
    float bw = pw * (footer_rows == 2 ? 0.30f : 0.24f);
    float gapb = pw * 0.04f;
    float total_w = footer_rows * bw + (footer_rows - 1) * gapb;
    float bx0 = px + (pw - total_w) * 0.5f;
    float by = content_max.y + (footer_h - bh) * 0.5f;
    return ImVec2(bx0 + f * (bw + gapb), by);
}

ImVec2 PanelLayout::footerButtonMax(int f, int footer_rows, int list_count) const {
    ImVec2 bmin = footerButtonMin(f, footer_rows, list_count);
    float bh = row_h * 0.95f;
    float bw = pw * (footer_rows == 2 ? 0.30f : 0.24f);
    return ImVec2(bmin.x + bw, bmin.y + bh);
}

PanelLayout PanelBase::beginPanel(const char* title, int list_count, int footer_rows, const Config& cfg) {
    PanelLayout L;
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    float W = vp->WorkSize.x, H = vp->WorkSize.y;
    bool light = (cfg.theme == "light");

    L.fade_col = light ? IM_COL32(240, 240, 240, 210) : IM_COL32(0, 0, 0, 210);
    L.panel_bg = light ? IM_COL32(250, 250, 252, 250) : IM_COL32(30, 32, 40, 250);
    L.panel_title = light ? IM_COL32(230, 232, 240, 255) : IM_COL32(44, 47, 60, 255);
    L.row_focus = light ? IM_COL32(210, 220, 240, 255) : IM_COL32(78, 82, 98, 255);
    L.row_hover = light ? IM_COL32(220, 230, 245, 255) : IM_COL32(58, 62, 78, 255);
    L.row_pressed = light ? IM_COL32(190, 200, 220, 255) : IM_COL32(98, 102, 118, 255);
    L.text_main = light ? IM_COL32(25, 25, 30, 255) : IM_COL32(230, 230, 230, 255);
    L.text_val = light ? IM_COL32(40, 40, 50, 255) : IM_COL32(240, 240, 240, 255);
    L.border_col = light ? IM_COL32(120, 125, 140, 255) : IM_COL32(150, 155, 170, 200);

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

    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(ImVec2(0, 0), ImVec2(W, H), L.fade_col);
    dl->AddRectFilled(L.panel_min, L.panel_max, L.panel_bg, 10.0f);
    dl->AddRectFilled(L.panel_min, ImVec2(L.panel_max.x, L.py + L.title_h),
                      L.panel_title, 10.0f, ImDrawFlags_RoundCornersTop);

    ImGui::PushFont(ui::g_font_tile);
    ImVec2 tt = ImGui::CalcTextSize(title);
    dl->AddText(ImVec2(L.px + (L.pw - tt.x) * 0.5f, L.py + (L.title_h - tt.y) * 0.5f),
                L.text_main, title);
    ImGui::PopFont();

    return L;
}

void PanelBase::drawRow(int id, const RowData& row, const PanelLayout& L, 
                        bool focused, bool hovered) {
    ImVec2 rmin = L.rowMin(id);
    ImVec2 rmax = L.rowMax(id);
    ImDrawList* dl = ImGui::GetWindowDrawList();

    if (focused) {
        dl->AddRectFilled(rmin, rmax, L.row_focus);
    } else if (hovered) {
        dl->AddRectFilled(rmin, rmax, L.row_hover);
    }

    ImGui::PushFont(ui::g_font_tile);
    ImVec2 tt = ImGui::CalcTextSize("X");

    void* ricon = nullptr; // Se obtendría de ui_icons si se pasa como parámetro
    float isz = L.row_h * 0.6f;
    float lx = L.content_min.x + L.pad;
    
    if (ricon) {
        dl->AddImage((ImTextureID)ricon,
                     ImVec2(lx, rmin.y + (L.row_h - isz) * 0.5f),
                     ImVec2(lx + isz, rmin.y + (L.row_h + isz) * 0.5f),
                     ImVec2(0, 0), ImVec2(1, 1), L.text_main);
        lx += isz + L.pad * 0.8f;
    }
    
    dl->AddText(ImVec2(lx, rmin.y + (L.row_h - tt.y) * 0.5f), L.text_main, row.label);

    if (!row.value.empty()) {
        std::string val = row.value;
        if (row.adjustable) val = "<  " + val + "  >";
        ImVec2 vs = ImGui::CalcTextSize(val.c_str());
        dl->AddText(ImVec2(L.content_max.x - L.pad - vs.x, rmin.y + (L.row_h - vs.y) * 0.5f),
                    L.text_val, val.c_str());
    }
    
    ImGui::PopFont();
}

void PanelBase::drawFooterButton(int id, const char* label, const PanelLayout& L,
                                  int button_index, int total_buttons, int list_count,
                                  bool focused, bool hovered) {
    ImVec2 bmin = L.footerButtonMin(button_index, total_buttons, list_count);
    ImVec2 bmax = L.footerButtonMax(button_index, total_buttons, list_count);
    float bh = bmax.y - bmin.y;
    float bw = bmax.x - bmin.x;
    ImDrawList* dl = ImGui::GetWindowDrawList();

    if (focused) {
        dl->AddRectFilled(bmin, bmax, L.row_focus, 6.0f);
    } else if (hovered) {
        dl->AddRectFilled(bmin, bmax, L.row_hover, 6.0f);
    }
    dl->AddRect(bmin, bmax, L.border_col, 6.0f, 0, 2.0f);

    ImGui::PushFont(ui::g_font_tile);
    ImVec2 bs = ImGui::CalcTextSize(label);
    dl->AddText(ImVec2(bmin.x + (bw - bs.x) * 0.5f, bmin.y + (bh - bs.y) * 0.5f),
                L.text_main, label);
    ImGui::PopFont();
}

} // namespace ui::panels