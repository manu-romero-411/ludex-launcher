#pragma once
#include <imgui.h>
#include "config.h"

namespace ui::widgets {

class EdgeFades {
public:
    void draw(const Config& cfg, float W, float H);
};

} // namespace ui::widgets