#include "pan_pin.h"
#include "../../core/i18n.h"

namespace ui::panels {
namespace {
RowDefinition digitRow(ShellState &st, int d) {
  RowDefinition r;
  r.label = std::to_string(d);
  r.kind = RowKind::Activator;
  r.icon = RowIcon::None;
  r.on_select = [d](ShellState &s, Config &, const ShellActions &) {
    if (s.pin_buffer.size() < 16) s.pin_buffer += (char)('0' + d);
  };
  return r;
}
} // namespace

PanelSpec makePinPanelSpec(ShellState &st, const ShellActions &actions) {
  PanelSpec spec;
  spec.pin_panel = true;
  spec.title = _("PIN") + std::string(" · ") +
               (st.pin_name.empty() ? st.pin_mac : st.pin_name);
  spec.focus_ptr = &st.pin_focus;
  spec.scroll_ptr = &st.pin_scroll;
  spec.on_back = [](ShellState &s, Config &) {
    if (s.show_pin) { // cancel
      s.show_pin = false;
    }
  };
  // El propio on_back no conoce actions; el cancel real lo gestiona
  // ui_input_handlers / application vía bluetooth_cancel_pin (ver más abajo).

  // Fila de visualización del buffer
  RowDefinition buf;
  buf.kind = RowKind::Label;
  buf.label = _("PIN");
  buf.get_value = [&st](const Config &) {
    return st.pin_buffer.empty() ? std::string("_ _ _ _") : st.pin_buffer;
  };
  spec.rows.push_back(buf);

  for (int d = 1; d <= 9; ++d) spec.rows.push_back(digitRow(st, d));
  spec.rows.push_back(digitRow(st, 0));

  RowDefinition del;
  del.label = _("DELETE");
  del.kind = RowKind::Activator;
  del.icon = RowIcon::None;
  del.on_select = [](ShellState &s, Config &, const ShellActions &) {
    if (!s.pin_buffer.empty()) s.pin_buffer.pop_back();
  };
  spec.rows.push_back(del);

  RowDefinition ok;
  ok.label = _("OK");
  ok.kind = RowKind::Activator;
  ok.icon = RowIcon::Bluetooth;
  ok.on_select = [](ShellState &s, Config &, const ShellActions &a) {
    if (s.pin_buffer.empty()) return;
    if (a.bluetooth_submit_pin) a.bluetooth_submit_pin(s.pin_mac, s.pin_buffer);
    s.show_pin = false;
  };
  spec.rows.push_back(ok);

  RowDefinition cancel;
  cancel.label = _("CANCEL");
  cancel.kind = RowKind::Footer;
  cancel.icon = RowIcon::Exit;
  cancel.on_select = [](ShellState &s, Config &, const ShellActions &a) {
    if (a.bluetooth_cancel_pin) a.bluetooth_cancel_pin();
    s.show_pin = false;
  };
  spec.rows.push_back(cancel);
  return spec;
}
} // namespace ui::panels