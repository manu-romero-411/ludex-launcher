#include "pan_settings.h"
#include "../../assets.h"
#include <algorithm>

namespace ui::panels {

PanelSpec makeSettingsPanelSpec(ShellState& st, Config& cfg, const ShellActions& actions) {
    PanelSpec spec;
    spec.title = "SETTINGS";
    spec.focus_ptr = &st.settings_focus;
    spec.on_back = [](ShellState& s, Config& c) {
        c.save(c.ini_path);
        s.show_settings = false;
    };

    spec.rows = {
        {"APP DRAWER MODE", RowKind::CycleString, RowIcon::None,
            [](const Config& c) { return c.side; },
            [](Config& c, int) {
                if (c.side == "left") c.side = "right";
                else if (c.side == "right") c.side = "top";
                else if (c.side == "top") c.side = "bottom";
                else c.side = "left";
            }, nullptr},

        {"# VISIBLE ELEMENTS", RowKind::NumericStep, RowIcon::None,
            [](const Config& c) { return std::to_string(c.visible_items); },
            [](Config& c, int d) {
                c.visible_items = std::clamp(c.visible_items + 2 * d, 5, 11);
                if (c.visible_items % 2 == 0) c.visible_items--;
            }, nullptr},

        {"SHOW GAMEPAD INDICATORS", RowKind::Toggle, RowIcon::None,
            [](const Config& c) -> std::string { return c.show_player_indicators ? "YES" : "NO"; },
            [](Config& c, int) { c.show_player_indicators = !c.show_player_indicators; },
            nullptr},

        {"COLOUR SCHEME", RowKind::CycleString, RowIcon::None,
            [](const Config& c) { return c.theme; },
            [](Config& c, int) { c.theme = (c.theme == "dark") ? "light" : "dark"; },
            nullptr},

        {"HELP ICONS", RowKind::Activator, RowIcon::None,
            [](const Config& c) { return c.help_icons; }, nullptr,
            [&actions](ShellState&, Config& c, const ShellActions&) {
                c.help_icons = (c.help_icons == "xbox") ? "playstation"
                             : (c.help_icons == "playstation") ? "none" : "xbox";
                if (actions.reload_ui_icons) actions.reload_ui_icons();
            }},

        {"ALL PLAYERS CONTROL UI", RowKind::Toggle, RowIcon::None,
            [](const Config& c) -> std::string { return c.all_players_ui ? "YES" : "NO"; },
            [](Config& c, int) { c.all_players_ui = !c.all_players_ui; },
            nullptr},

        {"GO BACK", RowKind::Footer, RowIcon::Exit,
            nullptr, nullptr,
            [](ShellState& s, Config& c, const ShellActions&) {
                c.save(c.ini_path);
                s.show_settings = false;
            }},
    };
    return spec;
}

} // namespace ui::panels