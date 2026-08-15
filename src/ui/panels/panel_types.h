#pragma once
#include <functional>
#include <string>
#include <vector>
#include "../../core/config.h"
#include "../shell/shell_state.h"

namespace ui::panels {

enum class RowKind {
    Label,       // Solo texto, no seleccionable
    Toggle,      // YES/NO cíclico
    CycleString, // Cambia entre valores predefinidos (p.ej. side, theme)
    NumericStep, // +/- numérico
    Activator,   // Ejecuta una acción al seleccionar
    Footer       // Botón de pie (Back, etc.)
};

struct RowDefinition {
    std::string label;
    RowKind kind = RowKind::Activator;
    RowIcon icon = RowIcon::None;

    // Getter para mostrar el valor actual (para Toggle, CycleString, NumericStep)
    std::function<std::string(const Config&)> get_value;

    // Ajuste left/right (para NumericStep, CycleString)
    std::function<void(Config&, int delta)> adjust;

    // Acción al seleccionar (para Activator, Footer, Toggle, CycleString, NumericStep)
    std::function<void(ShellState&, Config&, const ShellActions&)> on_select;
};

struct PanelSpec {
    std::string title;
    std::vector<RowDefinition> rows;
    int* focus_ptr = nullptr;                // Puntero al índice de foco actual
    std::function<void(ShellState&, Config&)> on_back; // Qué hacer al pulsar Back
};

} // namespace ui::panels