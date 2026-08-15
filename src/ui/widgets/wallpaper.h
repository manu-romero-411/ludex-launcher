#pragma once
#include <imgui.h>
#include "../shell/shell_state.h"

namespace ui::widgets {

void drawWallpaperLayer(ImDrawList* dl, float W, float H, 
                        const WallpaperLayer& L, ImU32 tint);

} // namespace ui::widgets