#pragma once
#include <imgui.h>
#include <string>
#include "../core/app_discovery.h"
#include "../core/config.h"

namespace ui {

// Fonts globales (cargadas una vez)
extern ImFont* g_font_tile;
extern ImFont* g_font_clock;
extern ImFont* g_font_date;
extern ImFont* g_font_hint;

void loadShellFonts(const Config& cfg, float screen_h);

// Utilidades
std::string upper(std::string s);
ImU32 colScaled(const TileColor& c, float f, int a = 255);
float flerp(float a, float b, float t);
float smooth(float t);

} // namespace ui