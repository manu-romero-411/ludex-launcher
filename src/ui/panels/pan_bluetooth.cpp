#include "pan_bluetooth.h"
#include "../../core/bluetooth_manager.h"

namespace ui::panels {
namespace {
RowIcon iconForKind(BtDeviceKind k) {
    switch (k) {
    case BtDeviceKind::Gamepad: return RowIcon::Gamepad;
    case BtDeviceKind::Audio:   return RowIcon::Headset;
    default:                    return RowIcon::Gear;
    }
}
const ImU32 COL_CONNECTED    = IM_COL32(60, 200, 90, 255);
const ImU32 COL_DISCONNECTED = IM_COL32(225, 70, 70, 255);
} // namespace

PanelSpec makeBluetoothPanelSpec(ShellState &st, Config &cfg,
                                 const ShellActions &actions) {
    PanelSpec spec;
    spec.title = "BLUETOOTH";
    spec.focus_ptr = &st.bluetooth_focus;
    spec.scroll_ptr = &st.bluetooth_scroll;
    spec.on_back = [](ShellState &s, Config &) { s.show_bluetooth = false; };

    // --- Fila de escaneo, con icono bluetooth ---
    {
        RowDefinition row;
        row.label = "SCAN NEW DEVICE...";
        row.kind = RowKind::Activator;
        row.icon = RowIcon::Bluetooth;
        row.on_select = [&actions](ShellState &s, Config &, const ShellActions &) {
            if (actions.bluetooth_scan)
                actions.bluetooth_scan();
            s.show_bluetooth = false;
            s.show_bluetooth_scan = true;
            s.bluetooth_scan_focus = 0;
        };
        spec.rows.push_back(row);
    }

    // --- Dispositivos paired: icono por tipo, color por estado ---
    if (actions.bluetooth_devices) {
        for (const auto &dev : actions.bluetooth_devices()) {
            RowDefinition row;
            row.kind = RowKind::Activator;
            row.label = dev.name;                    // sin [CONNECTED]
            row.icon = iconForKind(dev.kind);
            row.icon_col = dev.connected ? COL_CONNECTED : COL_DISCONNECTED;
            if (dev.connected) {
                row.on_select = [&actions, mac = dev.mac](ShellState &, Config &,
                                const ShellActions &) {
                    if (actions.bluetooth_disconnect)
                        actions.bluetooth_disconnect(mac);
                };
            } else {
                row.on_select = [&actions, mac = dev.mac](ShellState &, Config &,
                                const ShellActions &) {
                    if (actions.bluetooth_connect)
                        actions.bluetooth_connect(mac);
                };
            }
            spec.rows.push_back(row);
        }
    }

    // --- Footer ---
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