#pragma once
#include "../../core/config.h"
#include "../shell/shell_state.h"

namespace ui::widgets {

class TileCarousel {
public:
    void draw(ShellState& state, const Config& cfg, const ShellActions& actions,
              float W, float H, bool panel_open);
};

} // namespace ui