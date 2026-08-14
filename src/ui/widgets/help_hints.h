#pragma once
#include <imgui.h>
#include "config.h"
#include "shell_state.h"

namespace ui::widgets {

class HelpHints {
public:
    void draw(const ShellState& state, const Config& cfg, const ImGuiViewport* vp);
};

} // namespace ui::widgets