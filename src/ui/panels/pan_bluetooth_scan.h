#pragma once
#include "panel_types.h"

namespace ui::panels {
PanelSpec makeBluetoothScanPanelSpec(ShellState& st, Config& cfg,
                                     const ShellActions& actions);
} // namespace ui::panels

