#include "wallpaper.h"
#include <algorithm>
#include <cmath>

namespace ui {

void drawWallpaperLayer(ImDrawList* dl, float W, float H, const WallpaperLayer& L, ImU32 tint) {
    if (!L.texture || L.w <= 0 || L.h <= 0) return;

    float iw = (float)L.w, ih = (float)L.h;
    float cover_scale = std::max(W / iw, H / ih);
    float zoom = L.kb_scale;
    float total_scale = cover_scale * zoom;
    float vis_w = W / total_scale;
    float vis_h = H / total_scale;

    float cx = 0.5f + L.kb_pan_x;
    float cy = 0.5f + L.kb_pan_y;
    cx = std::clamp(cx, vis_w / (2.0f * iw), 1.0f - vis_w / (2.0f * iw));
    cy = std::clamp(cy, vis_h / (2.0f * ih), 1.0f - vis_h / (2.0f * ih));

    ImVec2 uv0(cx - vis_w / (2.0f * iw), cy - vis_h / (2.0f * ih));
    ImVec2 uv1(cx + vis_w / (2.0f * iw), cy + vis_h / (2.0f * ih));

    dl->AddImage((ImTextureID)L.texture, ImVec2(0, 0), ImVec2(W, H), uv0, uv1, tint);
}

} // namespace ui