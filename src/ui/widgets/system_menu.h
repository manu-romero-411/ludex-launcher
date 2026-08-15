#pragma once
#include <imgui.h>
#include "../../core/config.h"
#include "../shell/shell_state.h"
#include "../shell/shell_actions.h"

namespace ui::widgets {

class SystemMenu {
public:
    void draw(ShellState& state, const Config& cfg, const ShellActions& actions,
              float W, float H);
};

} // namespace ui::widgets