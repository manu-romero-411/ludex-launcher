#pragma once
#include <imgui.h>
#include <vector>
#include "../../core/config.h"
#include "../../input/input_manager.h"
#include "../shell/shell_state.h"

namespace ui::widgets {

class PlayerIndicators {
public:
    void draw(const ShellState& state, const Config& cfg,
              const std::vector<InputManager::PlayerStatus>& players,
              const ImGuiViewport* vp);
private:
    void drawGamepadGlyph(ImDrawList* dl, ImVec2 min, ImVec2 max, ImU32 col);
};

} // namespace ui::widgets