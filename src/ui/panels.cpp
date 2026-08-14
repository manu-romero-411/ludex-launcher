#include "panels.h"
#include "ui_common.h"
#include "input_manager.h"
#include <algorithm>

namespace Ids {
constexpr int PANEL_ROW_BASE = 9000;
constexpr int PANEL_FOOTER_BASE = 9100;
constexpr int CONTROLLER_ROW_BASE = 9300;
constexpr int CONTROLLER_BACK = 9400;
}

namespace ui {

// ============================================================================
// PanelLayout
// ============================================================================
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

PanelLayout beginPanel(const char* title, int list_count, int footer_rows, const Config& cfg) {
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

    ImGui::PushFont(g_font_tile);
    ImVec2 tt = ImGui::CalcTextSize(title);
    dl->AddText(ImVec2(L.px + (L.pw - tt.x) * 0.5f, L.py + (L.title_h - tt.y) * 0.5f),
                L.text_main, title);
    ImGui::PopFont();

    return L;
}

// ============================================================================
// Settings/Power rows definition
// ============================================================================
struct SRow {
    const char* label;
    bool adjustable;
    std::string (*get)(const Config&);
    void (*adjust)(Config&, int);
    void (*activate)(ShellState&, Config&, const ShellActions&);
    RowIcon icon;
};

static const SRow kSettingsRows[] = {
    {"APP DRAWER MODE", true, [](const Config& c) { return c.side; },
     [](Config& c, int) {
         if (c.side == "left") c.side = "right";
         else if (c.side == "right") c.side = "top";
         else if (c.side == "top") c.side = "bottom";
         else c.side = "left";
     }, nullptr, RowIcon::None},
    {"# VISIBLE ELEMENTS", true,
     [](const Config& c) { return std::to_string(c.visible_items); },
     [](Config& c, int d) {
         c.visible_items = std::clamp(c.visible_items + 2 * d, 5, 11);
         if (c.visible_items % 2 == 0) c.visible_items--;
     }, nullptr, RowIcon::None},
    {"SHOW GAMEPAD INDICATORS", true,
     [](const Config& c) -> std::string { return c.show_player_indicators ? "YES" : "NO"; },
     [](Config& c, int) { c.show_player_indicators = !c.show_player_indicators; },
     nullptr, RowIcon::None},
    {"COLOUR SCHEME", true, [](const Config& c) { return c.theme; },
     [](Config& c, int) { c.theme = (c.theme == "dark") ? "light" : "dark"; },
     nullptr, RowIcon::None},
    {"HELP ICONS", false, [](const Config& c) { return c.help_icons; }, nullptr,
     [](ShellState&, Config& c, const ShellActions& a) {
         c.help_icons = (c.help_icons == "xbox") ? "playstation"
                      : (c.help_icons == "playstation") ? "none" : "xbox";
         if (a.reload_ui_icons) a.reload_ui_icons();
     }, RowIcon::None},
    {"ALL PLAYERS CONTROL UI", true,
     [](const Config& c) -> std::string { return c.all_players_ui ? "YES" : "NO"; },
     [](Config& c, int) { c.all_players_ui = !c.all_players_ui; }, nullptr, RowIcon::None},
    {"GO BACK", false, nullptr, nullptr,
     [](ShellState& s, Config& c, const ShellActions&) {
         c.save(c.ini_path);
         s.show_settings = false;
     }, RowIcon::None},
};

static const SRow kPowerRows[] = {
    {"SLEEP", false, nullptr, nullptr,
     [](ShellState& s, Config&, const ShellActions& a) {
         if (a.suspend) a.suspend();
         s.show_power = false;
     }, RowIcon::Suspend},
    {"REBOOT", false, nullptr, nullptr,
     [](ShellState& s, Config&, const ShellActions& a) {
         if (a.reboot) a.reboot();
         s.show_power = false;
     }, RowIcon::Restart},
    {"POWER OFF", false, nullptr, nullptr,
     [](ShellState& s, Config&, const ShellActions& a) {
         if (a.poweroff) a.poweroff();
         s.show_power = false;
     }, RowIcon::Shutdown},
    {"GO BACK", false, nullptr, nullptr,
     [](ShellState& s, Config&, const ShellActions&) { s.show_power = false; }, RowIcon::Exit},
};

constexpr int kSettingsCount = (int)(sizeof(kSettingsRows) / sizeof(kSettingsRows[0]));
constexpr int kPowerCount = (int)(sizeof(kPowerRows) / sizeof(kPowerRows[0]));

// ============================================================================
// SettingsPanel
// ============================================================================
void SettingsPanel::draw(ShellState& st, Config& cfg, const ShellActions& actions, bool settings) {
    const SRow* rows = settings ? kSettingsRows : kPowerRows;
    int count = settings ? kSettingsCount : kPowerCount;
    int focus = settings ? st.settings_focus : st.power_focus;
    const char* title = settings ? "SETTINGS" : "SHUTDOWN...";
    int footer_rows = 1;
    int list_count = count - footer_rows;

    PanelLayout L = beginPanel(title, list_count, footer_rows, cfg);
    ImDrawList* dl = ImGui::GetWindowDrawList();

    ImGui::PushFont(g_font_tile);
    ImVec2 tt = ImGui::CalcTextSize("X");

    for (int i = 0; i < list_count; ++i) {
        ImVec2 rmin = L.rowMin(i);
        ImVec2 rmax = L.rowMax(i);
        ImGui::PushID(Ids::PANEL_ROW_BASE + i);
        ImGui::SetCursorScreenPos(rmin);
        ImGui::InvisibleButton("##row", ImVec2(rmax.x - rmin.x, rmax.y - rmin.y));
        bool hovered = ImGui::IsItemHovered();
        bool active = ImGui::IsItemActive();

        if (ImGui::IsItemClicked()) {
            (settings ? st.settings_focus : st.power_focus) = i;
            if (rows[i].activate) rows[i].activate(st, cfg, actions);
            else if (rows[i].adjust) rows[i].adjust(cfg, +1);
        }
        ImGui::PopID();

        if (active) {
            dl->AddRectFilled(rmin, rmax, L.row_pressed);
        } else if (i == focus) {
            dl->AddRectFilled(rmin, rmax, L.row_focus);
        } else if (hovered) {
            dl->AddRectFilled(rmin, rmax, L.row_hover);
        }

        void* ricon = st.ui_icons.byIndex(rows[i].icon);
        float isz = L.row_h * 0.6f;
        float lx = L.content_min.x + L.pad;
        if (ricon) {
            dl->AddImage((ImTextureID)ricon,
                         ImVec2(lx, rmin.y + (L.row_h - isz) * 0.5f),
                         ImVec2(lx + isz, rmin.y + (L.row_h + isz) * 0.5f),
                         ImVec2(0, 0), ImVec2(1, 1), L.text_main);
            lx += isz + L.pad * 0.8f;
        }
        dl->AddText(ImVec2(lx, rmin.y + (L.row_h - tt.y) * 0.5f), L.text_main, rows[i].label);

        if (rows[i].get) {
            std::string val = rows[i].get(cfg);
            if (rows[i].adjustable) val = "<  " + val + "  >";
            ImVec2 vs = ImGui::CalcTextSize(val.c_str());
            dl->AddText(ImVec2(L.content_max.x - L.pad - vs.x, rmin.y + (L.row_h - vs.y) * 0.5f),
                        L.text_val, val.c_str());
        }
    }

    for (int f = 0; f < footer_rows; ++f) {
        int i = list_count + f;
        ImVec2 bmin = L.footerButtonMin(f, footer_rows, list_count);
        ImVec2 bmax = L.footerButtonMax(f, footer_rows, list_count);
        float bh = bmax.y - bmin.y;
        float bw = bmax.x - bmin.x;

        ImGui::PushID(Ids::PANEL_FOOTER_BASE + i);
        ImGui::SetCursorScreenPos(bmin);
        ImGui::InvisibleButton("##fbtn", ImVec2(bw, bh));
        bool hovered = ImGui::IsItemHovered();
        bool active = ImGui::IsItemActive();

        if (ImGui::IsItemClicked()) {
            (settings ? st.settings_focus : st.power_focus) = i;
            if (rows[i].activate) rows[i].activate(st, cfg, actions);
        }
        ImGui::PopID();

        if (active) {
            dl->AddRectFilled(bmin, bmax, L.row_pressed, 6.0f);
        } else if (focus == i) {
            dl->AddRectFilled(bmin, bmax, L.row_focus, 6.0f);
        } else if (hovered) {
            dl->AddRectFilled(bmin, bmax, L.row_hover, 6.0f);
        }
        dl->AddRect(bmin, bmax, L.border_col, 6.0f, 0, 2.0f);

        const char* blabel = rows[i].label;
        ImVec2 bs = ImGui::CalcTextSize(blabel);
        dl->AddText(ImVec2(bmin.x + (bw - bs.x) * 0.5f, bmin.y + (bh - bs.y) * 0.5f),
                    L.text_main, blabel);
    }
    ImGui::PopFont();
}

// ============================================================================
// ControllersPanel
// ============================================================================
static std::string assignmentLabel(const Config& cfg,
                                   const std::vector<InputManager::DeviceInfo>& devs, int p) {
    if (cfg.controller_guid[p].empty()) return "DEFAULT";
    for (const auto& d : devs)
        if (d.guid == cfg.controller_guid[p])
            return "#" + std::to_string(d.sdl_index) + " " + d.name;
    return "OFFLINE (" + cfg.controller_name[p] + ")";
}

void ControllersPanel::draw(ShellState& st, Config& cfg, const ShellActions& actions) {
    auto devs = actions.devices ? actions.devices() : std::vector<InputManager::DeviceInfo>{};
    bool picking = st.controller_pick_player >= 0;

    std::vector<std::pair<std::string, std::string>> rows;
    std::string title;
    if (!picking) {
        title = "CONTROLLER ASSIGNMENT";
        for (int p = 0; p < 8; ++p)
            rows.push_back({"PLAYER " + std::to_string(p + 1) + " PAD", assignmentLabel(cfg, devs, p)});
    } else {
        title = "PAD FOR PLAYER " + std::to_string(st.controller_pick_player + 1);
        rows.push_back({"DEFAULT", ""});
        for (const auto& d : devs)
            rows.push_back({"#" + std::to_string(d.sdl_index) + " " + d.name, ""});
    }

    int list_count = (int)rows.size();
    int focus = picking ? st.controller_pick_focus : st.controllers_focus;
    PanelLayout L = beginPanel(title.c_str(), list_count, 1, cfg);
    ImDrawList* dl = ImGui::GetWindowDrawList();

    ImGui::PushFont(g_font_tile);
    ImVec2 tt = ImGui::CalcTextSize("X");

    for (int i = 0; i < list_count; ++i) {
        ImVec2 rmin = L.rowMin(i);
        ImVec2 rmax = L.rowMax(i);
        ImGui::PushID(Ids::CONTROLLER_ROW_BASE + i);
        ImGui::SetCursorScreenPos(rmin);
        ImGui::InvisibleButton("##crow", ImVec2(rmax.x - rmin.x, rmax.y - rmin.y));
        bool hovered = ImGui::IsItemHovered();

        if (ImGui::IsItemClicked()) {
            if (picking) st.controller_pick_focus = i;
            else st.controllers_focus = i;
            controllersInput(st, cfg, actions, UiAction::Select);
        }
        ImGui::PopID();

        if (i == focus) dl->AddRectFilled(rmin, rmax, L.row_focus);
        else if (hovered) dl->AddRectFilled(rmin, rmax, L.row_hover);

        dl->AddText(ImVec2(L.content_min.x + L.pad, rmin.y + (L.row_h - tt.y) * 0.5f),
                    L.text_main, rows[i].first.c_str());
        if (!rows[i].second.empty()) {
            ImVec2 vs = ImGui::CalcTextSize(rows[i].second.c_str());
            dl->AddText(ImVec2(L.content_max.x - L.pad - vs.x, rmin.y + (L.row_h - vs.y) * 0.5f),
                        L.text_val, rows[i].second.c_str());
        }
    }

    ImVec2 bmin = L.footerButtonMin(0, 1, list_count);
    ImVec2 bmax = L.footerButtonMax(0, 1, list_count);
    float bh = bmax.y - bmin.y;
    float bw = bmax.x - bmin.x;

    ImGui::PushID(Ids::CONTROLLER_BACK);
    ImGui::SetCursorScreenPos(bmin);
    ImGui::InvisibleButton("##cback", ImVec2(bw, bh));
    bool hovered = ImGui::IsItemHovered();
    if (ImGui::IsItemClicked()) {
        if (picking) st.controller_pick_player = -1;
        else st.show_controllers = false;
    }
    ImGui::PopID();

    if (focus == list_count) dl->AddRectFilled(bmin, bmax, L.row_focus, 6);
    else if (hovered) dl->AddRectFilled(bmin, bmax, L.row_hover, 6);
    dl->AddRect(bmin, bmax, L.border_col, 6, 0, 2);

    ImVec2 bs = ImGui::CalcTextSize("BACK");
    dl->AddText(ImVec2(bmin.x + (bw - bs.x) * 0.5f, bmin.y + (bh - bs.y) * 0.5f),
                L.text_main, "BACK");
    ImGui::PopFont();
}

} // namespace ui