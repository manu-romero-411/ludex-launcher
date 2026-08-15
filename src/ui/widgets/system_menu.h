#pragma once
#include <imgui.h>
#include "config.h"
#include "shell_state.h"
#include "shell_actions.h"

namespace ui::widgets {

class SystemMenu {
public:
    void draw(ShellState& state, const Config& cfg, const ShellActions& actions,
              float W, float H);
};

} // namespace ui::widgets