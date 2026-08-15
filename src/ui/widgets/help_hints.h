#pragma once
#include <imgui.h>
#include "../../core/config.h"
#include "../shell/shell_state.h"

namespace ui::widgets {

class HelpHints {
public:
    void draw(const ShellState& state, const Config& cfg, const ImGuiViewport* vp);
};

} // namespace ui::widgets