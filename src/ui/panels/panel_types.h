#pragma once
#include <functional>
#include <string>
#include <vector>
#include <imgui.h>
#include "../../core/config.h"
#include "../shell/shell_state.h"

namespace ui::panels {

enum class RowKind {
    Label, Toggle, CycleString, NumericStep, Activator, Footer
};

struct RowDefinition {
    std::string label;
    RowKind kind = RowKind::Activator;
    RowIcon icon = RowIcon::None;
    std::function<std::string(const Config&)> get_value;
    std::function<void(Config&, int delta)> adjust;
    std::function<void(ShellState&, Config&, const ShellActions&)> on_select;
    ImU32 icon_col = 0;   // 0 = usar el color de texto por defecto
  std::string tag;          // NUEVO: payload libre (p.ej. MAC del dispositivo)

};

struct PanelSpec {
    std::string title;
    std::vector<RowDefinition> rows;
    int* focus_ptr = nullptr;
    float* scroll_ptr = nullptr;
    std::function<void(ShellState&, Config&)> on_back;
      bool pin_panel = false;   // NUEVO: este panel es el modal de PIN

};

} // namespace ui::panels