#include "pan_bluetooth_scan.h"

namespace ui::panels {

PanelSpec makeBluetoothScanPanelSpec(ShellState &st, Config &cfg,
                                     const ShellActions &actions) {
    PanelSpec spec;
    spec.title = "BLUETOOTH SCAN";
    spec.focus_ptr = &st.bluetooth_scan_focus;
    spec.on_back = [](ShellState &s, Config &) {
        s.show_bluetooth_scan = false;
        s.show_bluetooth = true;
    };

    bool scanning = actions.bluetooth_scanning && actions.bluetooth_scanning();

    if (scanning) {
        int rem = actions.bluetooth_scan_remaining ? actions.bluetooth_scan_remaining() : 0;
        RowDefinition row;
        row.label = "SCANNING... (" + std::to_string(rem) + "s)";
        row.kind = RowKind::Label;
        row.icon = RowIcon::None;
        spec.rows.push_back(row);
    }

    if (actions.bluetooth_discovered) {
        auto discovered = actions.bluetooth_discovered();
        for (const auto &dev : discovered) {
            RowDefinition row;
            row.label = dev.name;
            row.kind = RowKind::Activator;
            row.icon = RowIcon::None;
            row.on_select = [&actions, mac = dev.mac](
                                ShellState &s, Config &,
                                const ShellActions &) {
                if (actions.bluetooth_pair)
                    actions.bluetooth_pair(mac);
                if (actions.bluetooth_connect)
                    actions.bluetooth_connect(mac);
                s.show_bluetooth_scan = false;
                s.show_bluetooth = true;
            };
            spec.rows.push_back(row);
        }
    }

    RowDefinition back_row;
    back_row.label = "BACK";
    back_row.kind = RowKind::Footer;
    back_row.icon = RowIcon::Exit;
    back_row.on_select = [](ShellState &s, Config &,
                            const ShellActions &) {
        s.show_bluetooth_scan = false;
        s.show_bluetooth = true;
    };
    spec.rows.push_back(back_row);

    return spec;
}

} // namespace ui::panels