#include "pan_settings.h"
#include "../../core/i18n.h"
#include <algorithm>

namespace ui::panels {

PanelSpec makeSettingsPanelSpec(ShellState &st, Config &cfg,
                                const ShellActions &actions) {
  PanelSpec spec;
  spec.title = _("SETTINGS");
  spec.focus_ptr = &st.settings_focus;
  spec.scroll_ptr = &st.settings_scroll;
  spec.on_back = [](ShellState &s, Config &c) {
    c.save(c.ini_path);
    s.show_settings = false;
    s.show_system = true;
    s.system_focus = 0;
  };

  spec.rows = {
      {_("APP DRAWER MODE"), RowKind::CycleString, RowIcon::None,
       [](const Config &c) { return sideToString(c.side); },
       [](Config &c, int) {
         switch (c.side) {
         case LayoutSide::Left:
           c.side = LayoutSide::Right;
           break;
         case LayoutSide::Right:
           c.side = LayoutSide::Top;
           break;
         case LayoutSide::Top:
           c.side = LayoutSide::Bottom;
           break;
         case LayoutSide::Bottom:
           c.side = LayoutSide::Left;
           break;
         }
       },
       nullptr},

      {_("# VISIBLE ELEMENTS"), RowKind::NumericStep, RowIcon::None,
       [](const Config &c) { return std::to_string(c.visible_items); },
       [](Config &c, int d) {
         c.visible_items = std::clamp(c.visible_items + 2 * d, 5, 11);
         if (c.visible_items % 2 == 0)
           c.visible_items--;
       },
       nullptr},

      {_("SHOW GAMEPAD INDICATORS"), RowKind::Toggle, RowIcon::None,
       [](const Config &c) -> std::string {
         return c.show_player_indicators ? "YES" : "NO";
       },
       [](Config &c, int) {
         c.show_player_indicators = !c.show_player_indicators;
       },
       nullptr},

      {_("COLOUR SCHEME"), RowKind::CycleString, RowIcon::None,
       [](const Config &c) { return themeToString(c.theme); },
       [](Config &c, int) {
         c.theme = (c.theme == Theme::Dark) ? Theme::Light : Theme::Dark;
       },
       nullptr},

      {_("HELP ICONS"), RowKind::Activator, RowIcon::None,
       [](const Config &c) { return helpIconsToString(c.help_icons); }, nullptr,
       [&actions](ShellState &, Config &c, const ShellActions &) {
         switch (c.help_icons) {
         case HelpIcons::Xbox:
           c.help_icons = HelpIcons::PlayStation;
           break;
         case HelpIcons::PlayStation:
           c.help_icons = HelpIcons::None;
           break;
         case HelpIcons::None:
           c.help_icons = HelpIcons::Xbox;
           break;
         }
         if (actions.reload_ui_icons)
           actions.reload_ui_icons();
       }},

      {_("ALL PLAYERS CONTROL UI"), RowKind::Toggle, RowIcon::None,
       [](const Config &c) -> std::string {
         return c.all_players_ui ? "YES" : "NO";
       },
       [](Config &c, int) { c.all_players_ui = !c.all_players_ui; }, nullptr},

      {_("BACK"), RowKind::Footer, RowIcon::Exit, nullptr, nullptr,
       [](ShellState &s, Config &c, const ShellActions &) {
         c.save(c.ini_path);
         s.show_settings = false;
         s.show_system = true; // <-- AÑADIR
         s.system_focus = 0;   // <-- AÑADIR
       }},
  };
  return spec;
}

} // namespace ui::panels