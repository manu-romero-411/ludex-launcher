#pragma once
#include <imgui.h>
#include <vector>
#include "config.h"
#include "input_manager.h"
#include "shell_state.h"

namespace ui {

class SystemClock {
public:
    void draw(const Config& cfg, const ImGuiViewport* vp, bool left_side, bool bottom);
};

class EdgeFades {
public:
    void draw(const Config& cfg, float W, float H);
};

class HelpHints {
public:
    void draw(const ShellState& state, const Config& cfg, const ImGuiViewport* vp);
};

class PlayerIndicators {
public:
    void draw(const ShellState& state, const Config& cfg,
              const std::vector<InputManager::PlayerStatus>& players,
              const ImGuiViewport* vp);
private:
    void drawGamepadGlyph(ImDrawList* dl, ImVec2 min, ImVec2 max, ImU32 col);
};

} // namespace ui