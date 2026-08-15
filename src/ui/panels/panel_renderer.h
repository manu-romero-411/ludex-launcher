// src/ui/panels/panel_renderer.h
#pragma once
#include "panel_types.h"

namespace ui::panels {

// SOLO DIBUJA. No procesa input.
void drawGenericPanel(
    const PanelSpec& spec,
    ShellState& st,
    Config& cfg,
    const ShellActions& actions);

} // namespace ui::panels