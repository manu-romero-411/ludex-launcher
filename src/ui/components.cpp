#include "components.h"
#include "ui_common.h"
#include <algorithm>
#include <ctime>

namespace ui {

// ============================================================================
// SystemClock
// ============================================================================
void SystemClock::draw(const Config& cfg, const ImGuiViewport* vp, bool left_side, bool bottom) {
    std::time_t now = std::time(nullptr);
    std::tm* local = std::localtime(&now);
    char time_text[32], date_text[128];
    std::strftime(time_text, sizeof(time_text), "%H:%M:%S", local);
    std::strftime(date_text, sizeof(date_text), "%A %d de %B de %Y", local);

    float margin = vp->WorkSize.x * 0.018f;

    ImGui::PushFont(g_font_clock);
    ImVec2 ts = ImGui::CalcTextSize(time_text);
    ImGui::PopFont();
    ImGui::PushFont(g_font_date);
    ImVec2 ds = ImGui::CalcTextSize(date_text);
    ImGui::PopFont();

    float x_time = left_side ? vp->WorkSize.x - margin - ts.x : margin;
    float x_date = left_side ? vp->WorkSize.x - margin - ds.x : margin;
    float y_time, y_date;
    if (bottom) {
        y_date = vp->WorkSize.y - margin - ds.y;
        y_time = y_date - 6.0f - ts.y;
    } else {
        y_time = margin;
        y_date = margin + ts.y + 6.0f;
    }

    ImGui::PushFont(g_font_clock);
    ImGui::SetCursorScreenPos(ImVec2(x_time, y_time));
    ImGui::TextUnformatted(time_text);
    ImGui::PopFont();

    ImGui::PushFont(g_font_date);
    ImGui::SetCursorScreenPos(ImVec2(x_date, y_date));
    ImGui::TextColored(ImVec4(0.85f, 0.85f, 0.88f, 0.9f), "%s", date_text);
    ImGui::PopFont();
}

// ============================================================================
// EdgeFades
// ============================================================================
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

// ============================================================================
// HelpHints
// ============================================================================
void HelpHints::draw(const ShellState& state, const Config& cfg, const ImGuiViewport* vp) {
    float W = vp->WorkSize.x, H = vp->WorkSize.y;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    bool light = (cfg.theme == "light");
    ImU32 col = light ? IM_COL32(40, 40, 45, 220) : IM_COL32(235, 235, 235, 220);

    const UiIcons& ic = state.ui_icons;
    struct Seg {
        void* tex;
        const char* fallback;
        const char* label;
    };

    bool horiz = (cfg.side == "top" || cfg.side == "bottom");
    std::vector<Seg> segs;

    if (state.show_settings) {
        segs = {{ic.nav_v, "UP/DOWN", "NAVIGATE"}, {ic.nav_h, "LEFT/RIGHT", "CHANGE"},
                {ic.accept, "A", "OK"}, {ic.back, "B", "BACK"}};
    } else if (state.show_controllers) {
        segs = {{ic.nav_v, "UP/DOWN", "NAVIGATE"}, {ic.nav_h, "LEFT/RIGHT", "CHANGE"},
                {ic.accept, "A", "OK"}, {ic.back, "B", "BACK"}};
    } else if (state.show_power) {
        segs = {{ic.nav_v, "UP/DOWN", "NAVIGATE"}, {ic.accept, "A", "ACCEPT"}, {ic.back, "B", "BACK"}};
    } else if (state.menu_open) {
        segs = {{ic.nav_v, "UP/DOWN", "NAVIGATE"}, {ic.accept, "A", "ACCEPT"},
                {ic.back, "B / HOME", "CLOSE"}};
    } else {
        segs = {{horiz ? ic.nav_h : ic.nav_v, horiz ? "LEFT/RIGHT" : "UP/DOWN", "NAVIGATE"},
                {ic.accept, "A", "SELECT"}, {ic.back, "B", "BACK"},
                {ic.home, "HOME", "SYSTEM MENU"}};
    }

    ImGui::PushFont(g_font_hint);
    float gap_seg = W * 0.014f;
    float gap_it = W * 0.004f;

    struct M {
        std::string text;
        float tw, iw;
        ImVec2 ts;
    };
    std::vector<M> ms;
    float total = 0, base_h = 0;

    for (auto& s : segs) {
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
    bool hints_top = (cfg.side == "bottom");
    float y = hints_top ? margin : (H - margin - base_h);
    float x = (W - total) * 0.5f;

    for (size_t i = 0; i < segs.size(); ++i) {
        const auto& s = segs[i];
        const auto& m = ms[i];
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

// ============================================================================
// PlayerIndicators
// ============================================================================
void PlayerIndicators::draw(const ShellState& state, const Config& cfg,
                            const std::vector<InputManager::PlayerStatus>& players,
                            const ImGuiViewport* vp) {
    if (players.empty()) return;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    float W = vp->WorkSize.x, H = vp->WorkSize.y;
    bool light = (cfg.theme == "light");
    ImU32 idle = light ? IM_COL32(40, 40, 45, 220) : IM_COL32(220, 222, 228, 220);
    ImU32 active = IM_COL32(0, 255, 90, 255);

    bool gp_top = (cfg.side == "bottom");
    bool gp_right = (cfg.side != "right");
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

void PlayerIndicators::drawGamepadGlyph(ImDrawList* dl, ImVec2 min, ImVec2 max, ImU32 col) {
    float w = max.x - min.x, h = max.y - min.y;
    ImVec2 bmin(min.x, min.y + h * 0.22f);
    ImVec2 bmax(max.x, max.y - h * 0.10f);
    dl->AddRectFilled(bmin, bmax, col, h * 0.28f);
    dl->AddCircleFilled(ImVec2(min.x + w * 0.20f, min.y + h * 0.28f), h * 0.16f, col);
    dl->AddCircleFilled(ImVec2(max.x - w * 0.20f, min.y + h * 0.28f), h * 0.16f, col);
}

} // namespace ui