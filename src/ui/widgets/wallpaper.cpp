#include "wallpaper.h"
#include <algorithm>
#include <cmath>

namespace ui::widgets {

void drawWallpaperLayer(ImDrawList* dl, float W, float H, 
                        const WallpaperLayer& L, ImU32 tint) {
    // Validar que la textura sea válida
    if (!L.texture || L.w <= 0 || L.h <= 0)
        return;

    // Convertir dimensiones a float para cálculos
    float iw = (float)L.w;
    float ih = (float)L.h;

    // Escala de cover: asegúrate de que la textura cubra toda la pantalla
    float cover_scale = std::max(W / iw, H / ih);
    
    // Aplicar zoom Ken Burns
    float zoom = L.kb_scale;
    float total_scale = cover_scale * zoom;

    // Calcular área visible de la textura
    float vis_w = W / total_scale;
    float vis_h = H / total_scale;

    // Centro con desplazamiento Ken Burns
    float cx = 0.5f + L.kb_pan_x;
    float cy = 0.5f + L.kb_pan_y;

    // Asegúrate de que el área visible permanezca dentro de los límites de la textura
    cx = std::clamp(cx, vis_w / (2.0f * iw), 1.0f - vis_w / (2.0f * iw));
    cy = std::clamp(cy, vis_h / (2.0f * ih), 1.0f - vis_h / (2.0f * ih));

    // Coordenadas UV para el área visible
    ImVec2 uv0(cx - vis_w / (2.0f * iw), cy - vis_h / (2.0f * ih));
    ImVec2 uv1(cx + vis_w / (2.0f * iw), cy + vis_h / (2.0f * ih));

    // Dibujar la textura en toda la ventana con las coordenadas UV calculadas
    dl->AddImage(
        (ImTextureID)L.texture,  // Textura
        ImVec2(0, 0),            // Esquina superior izquierda en pantalla
        ImVec2(W, H),           // Esquina inferior derecha en pantalla
        uv0,                     // Coordenada UV superior izquierda
        uv1,                     // Coordenada UV inferior derecha
        tint                     // Color de tinte (para efectos de desvanecido)
    );
}

} // namespace ui::widgets