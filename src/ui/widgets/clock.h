#pragma once
#include <imgui.h>
#include "../../core/config.h"

namespace ui::widgets {

class Clock {
public:
    void draw(const Config& cfg, const ImGuiViewport* vp, bool left_side, bool bottom);
};

} // namespace ui::widgets