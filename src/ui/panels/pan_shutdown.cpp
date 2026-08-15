#include "pan_shutdown.h"

namespace ui::panels {

PanelSpec makeShutdownPanelSpec(ShellState& st, const ShellActions& actions) {
    PanelSpec spec;
    spec.title = "SHUTDOWN...";
    spec.focus_ptr = &st.power_focus;
    spec.on_back = [](ShellState& s, Config&) { s.show_power = false; };

    spec.rows = {
        {"SLEEP", RowKind::Activator, RowIcon::Suspend, nullptr, nullptr,
            [&actions](ShellState& s, Config&, const ShellActions&) {
                if (actions.suspend) actions.suspend();
                s.show_power = false;
            }},
        {"REBOOT", RowKind::Activator, RowIcon::Restart, nullptr, nullptr,
            [&actions](ShellState& s, Config&, const ShellActions&) {
                if (actions.reboot) actions.reboot();
                s.show_power = false;
            }},
        {"POWER OFF", RowKind::Activator, RowIcon::Shutdown, nullptr, nullptr,
            [&actions](ShellState& s, Config&, const ShellActions&) {
                if (actions.poweroff) actions.poweroff();
                s.show_power = false;
            }},
        {"GO BACK", RowKind::Footer, RowIcon::Exit, nullptr, nullptr,
            [](ShellState& s, Config&, const ShellActions&) { s.show_power = false; }},
    };
    return spec;
}

} // namespace ui::panels