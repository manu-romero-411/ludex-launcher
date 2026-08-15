#include "pan_shutdown.h"
#include "../../core/i18n.h"

namespace ui::panels {

PanelSpec makeShutdownPanelSpec(ShellState &st, const ShellActions &actions) {
  PanelSpec spec;
  spec.title = _("SHUTDOWN...");
  spec.focus_ptr = &st.power_focus;
  spec.scroll_ptr = &st.settings_scroll;

  spec.on_back = [](ShellState &s, Config &) {
    s.show_power = false;
    s.show_system = true; // <-- AÑADIR
    s.system_focus = 0;   // <-- AÑADIR
  };
  spec.rows = {
      {_("SLEEP"), RowKind::Activator, RowIcon::Suspend, nullptr, nullptr,
       [&actions](ShellState &s, Config &, const ShellActions &) {
         if (actions.suspend)
           actions.suspend();
         s.show_power = false;
       }},
      {_("REBOOT"), RowKind::Activator, RowIcon::Restart, nullptr, nullptr,
       [&actions](ShellState &s, Config &, const ShellActions &) {
         if (actions.reboot)
           actions.reboot();
         s.show_power = false;
       }},
      {_("POWER OFF"), RowKind::Activator, RowIcon::Shutdown, nullptr, nullptr,
       [&actions](ShellState &s, Config &, const ShellActions &) {
         if (actions.poweroff)
           actions.poweroff();
         s.show_power = false;
       }},
      {_("BACK"), RowKind::Footer, RowIcon::Exit, nullptr, nullptr,
       [](ShellState &s, Config &, const ShellActions &) {
         s.show_power = false;
         s.show_system = true; // <-- AÑADIR
         s.system_focus = 0;   // <-- AÑADIR
       }},
  };
  return spec;
}

} // namespace ui::panels