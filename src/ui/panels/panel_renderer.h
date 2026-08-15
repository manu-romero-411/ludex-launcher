#pragma once
#include "panel_types.h"

namespace ui::panels {

// SOLO DIBUJA. No procesa input.
void drawGenericPanel(
    const PanelSpec& spec,
    ShellState& st,
    Config& cfg,
    const ShellActions& actions);

// Procesa una acción de input sobre un panel.
// Se llama desde panelInput() / controllersInput().
void handlePanelAction(
    const PanelSpec& spec,
    ShellState& st,
    Config& cfg,
    const ShellActions& actions,
    UiAction a);

} // namespace ui::panels