#include "pan_bluetooth.h"

namespace ui::panels {

PanelSpec makeBluetoothPanelSpec(ShellState &st, Config &cfg,
                                 const ShellActions &actions) {
  PanelSpec spec;
  spec.title = "BLUETOOTH";
  spec.focus_ptr = &st.bluetooth_focus;
  spec.on_back = [](ShellState &s, Config &) { s.show_bluetooth = false; };

  bool scanning = actions.bluetooth_scanning && actions.bluetooth_scanning();

  // --- Fila de escaneo ---
  {
    RowDefinition row;
    row.label = "SCAN NEW DEVICE...";
    row.kind = RowKind::Activator;
    row.icon = RowIcon::None;
    row.on_select = [&actions](ShellState &s, Config &, const ShellActions &) {
      if (actions.bluetooth_scan)
        actions.bluetooth_scan();
      s.show_bluetooth = false;
      s.show_bluetooth_scan = true;
      s.bluetooth_scan_focus = 0;
    };
    spec.rows.push_back(row);
  }

  // --- Dispositivos paired ---
  if (actions.bluetooth_devices) {
    auto devices = actions.bluetooth_devices();
    for (const auto &dev : devices) {
      RowDefinition row;
      row.kind = RowKind::Activator;
      row.icon = RowIcon::None;

      // Indicador de conexión en el label
      std::string status = dev.connected ? "[CONNECTED] " : "";
      row.label = status + dev.name;

      if (dev.connected) {
        // Si está conectado: desconectar
        row.on_select = [&actions, mac = dev.mac](ShellState &, Config &,
                                                  const ShellActions &) {
          if (actions.bluetooth_disconnect)
            actions.bluetooth_disconnect(mac);
        };
      } else {
        // Si no está conectado: conectar
        row.on_select = [&actions, mac = dev.mac](ShellState &, Config &,
                                                  const ShellActions &) {
          if (actions.bluetooth_connect)
            actions.bluetooth_connect(mac);
        };
      }
      spec.rows.push_back(row);
    }
  }

  // --- Footer: BACK ---
  {
    RowDefinition back_row;
    back_row.label = "BACK";
    back_row.kind = RowKind::Footer;
    back_row.icon = RowIcon::Exit;
    back_row.on_select = [](ShellState &s, Config &, const ShellActions &) {
      s.show_bluetooth = false;
    };
    spec.rows.push_back(back_row);
  }

  return spec;
}

} // namespace ui::panels