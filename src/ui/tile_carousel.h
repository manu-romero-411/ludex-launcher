#pragma once
#include "config.h"
#include "shell_state.h"
#include "shell_ui.h"

namespace ui {

class TileCarousel {
public:
    void draw(ShellState& state, const Config& cfg, const ShellActions& actions,
              float W, float H, bool panel_open);
};

} // namespace ui