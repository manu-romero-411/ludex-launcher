#include "pan_mainmenu.h"
#include "../../core/i18n.h"

namespace ui::panels {

PanelSpec makeSystemPanelSpec(ShellState &st, const ShellActions &actions) {
    PanelSpec spec;
    spec.title = _("SYSTEM");
    spec.focus_ptr = &st.system_focus;
    spec.scroll_ptr = &st.system_scroll;
    spec.on_back = [](ShellState &s, Config &) { s.show_system = false; };

    spec.rows = {
        {_("SETTINGS"), RowKind::Activator, RowIcon::Settings, nullptr, nullptr,
         [&actions](ShellState &s, Config &, const ShellActions &) {
             s.show_system = false;
             if (actions.open_settings) actions.open_settings();
         }},
        {_("BLUETOOTH"), RowKind::Activator, RowIcon::Bluetooth, nullptr, nullptr,
         [&actions](ShellState &s, Config &, const ShellActions &) {
             s.show_system = false;
             if (actions.open_bluetooth) actions.open_bluetooth();
         }},
        {_("CONTROLLERS"), RowKind::Activator, RowIcon::Gamepad, nullptr, nullptr,
         [&actions](ShellState &s, Config &, const ShellActions &) {
             s.show_system = false;
             if (actions.open_controllers) actions.open_controllers();
         }},
        {_("QUIT TO DESKTOP"), RowKind::Activator, RowIcon::Exit, nullptr, nullptr,
         [&actions](ShellState &, Config &, const ShellActions &) {
             if (actions.quit) actions.quit();
         }},
        {_("SHUTDOWN..."), RowKind::Activator, RowIcon::Shutdown, nullptr, nullptr,
         [](ShellState &s, Config &, const ShellActions &) {
             s.show_system = false;
             s.show_power = true;
             s.power_focus = 0;
         }},
        {_("BACK"), RowKind::Footer, RowIcon::Exit, nullptr, nullptr,
         [](ShellState &s, Config &, const ShellActions &) { s.show_system = false; }},
    };
    return spec;
}

} // namespace ui::panels