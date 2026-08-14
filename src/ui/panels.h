#pragma once
#include "config.h"
#include "imgui.h"
#include "shell_state.h"
#include "shell_ui.h"

namespace ui {

struct PanelLayout {
    ImVec2 panel_min, panel_max;
    ImVec2 content_min, content_max;
    float title_h, row_h, footer_h, pad;
    float pw, ph, px, py;
    ImU32 fade_col, panel_bg, panel_title;
    ImU32 row_focus, row_hover, row_pressed;
    ImU32 text_main, text_val, border_col;

    ImVec2 rowMin(int i) const;
    ImVec2 rowMax(int i) const;
    ImVec2 footerButtonMin(int f, int footer_rows, int list_count) const;
    ImVec2 footerButtonMax(int f, int footer_rows, int list_count) const;
};

PanelLayout beginPanel(const char* title, int list_count, int footer_rows, const Config& cfg);

class SettingsPanel {
public:
    void draw(ShellState& st, Config& cfg, const ShellActions& actions, bool settings);
};

class ControllersPanel {
public:
    void draw(ShellState& st, Config& cfg, const ShellActions& actions);
};

} // namespace ui