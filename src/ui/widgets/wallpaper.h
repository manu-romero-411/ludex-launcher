#pragma once
#include <imgui.h>
#include "../render/wallpaper_manager.h"

namespace ui::widgets {

void drawWallpaperLayer(ImDrawList* dl, float W, float H, 
                        const WallpaperLayer& L, ImU32 tint);

} // namespace ui::widgets