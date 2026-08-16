#include "ui_common.h"
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <iostream>

namespace ui {
ImFont *g_font_tile = nullptr;
ImFont *g_font_panel_title = nullptr;
ImFont *g_font_panel_row = nullptr;
ImFont *g_font_clock = nullptr;
ImFont *g_font_date = nullptr;
ImFont *g_font_hint = nullptr;

void loadShellFonts(const Config &cfg, float screen_h) {
  ImGuiIO &io = ImGui::GetIO();
  io.Fonts->Clear();
  std::error_code ec;

  float size_tile = std::max(14.0f, screen_h * cfg.font_tile_pct);
  float size_clock = std::max(16.0f, screen_h * cfg.clock_pct);
  float size_date = std::max(12.0f, screen_h * cfg.date_pct);
  float size_hint = std::max(10.0f, screen_h * cfg.font_hint_pct);

  if (std::filesystem::exists(cfg.font_bold, ec)) {
    g_font_tile =
        io.Fonts->AddFontFromFileTTF(cfg.font_bold.c_str(), size_tile);
    if (!g_font_tile) {
      std::cerr << "[ludex] AddFontFromFileTTF falló para: " << cfg.font_bold
                << "\n";
    }
  }

  if (!g_font_tile) {
    std::cerr << "[ludex] Usando fuente por defecto para tiles\n";
    g_font_tile = io.Fonts->AddFontDefault();
  }

  // Título de panel: bold, ~30% más grande que el tile
  float size_panel_title = size_tile * 1.3f;
  if (std::filesystem::exists(cfg.font_bold, ec)) {
    g_font_panel_title =
        io.Fonts->AddFontFromFileTTF(cfg.font_bold.c_str(), size_panel_title);
  }
  if (!g_font_panel_title)
    g_font_panel_title = g_font_tile;

  // Filas de panel: regular, mismo tamaño que el tile
  if (std::filesystem::exists(cfg.font_regular, ec)) {
    g_font_panel_row =
        io.Fonts->AddFontFromFileTTF(cfg.font_regular.c_str(), size_tile);
  }
  if (!g_font_panel_row)
    g_font_panel_row = g_font_tile;

  ImFont *regular = nullptr;
  if (std::filesystem::exists(cfg.font_regular, ec)) {
    regular =
        io.Fonts->AddFontFromFileTTF(cfg.font_regular.c_str(), size_clock);
    if (!regular) {
      std::cerr << "[ludex] AddFontFromFileTTF falló para: " << cfg.font_regular
                << "\n";
    }
  }
  if (!regular) {
    std::cerr << "[ludex] Usando fuente por defecto para texto regular\n";
    regular = io.Fonts->AddFontDefault();
  }

  g_font_clock = regular;
  g_font_date =
      io.Fonts->AddFontFromFileTTF(cfg.font_regular.c_str(), size_date);
  if (!g_font_date)
    g_font_date = regular;
  g_font_hint =
      io.Fonts->AddFontFromFileTTF(cfg.font_regular.c_str(), size_hint);
  if (!g_font_hint)
    g_font_hint = regular;

  unsigned char *pixels;
  int width, height;
  io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
  if (width == 0 || height == 0) {
    std::cerr
        << "[ludex] ERROR: Font atlas vacío después de GetTexDataAsRGBA32\n";
  }
  std::cerr << "[ludex] Fuentes cargadas: tile="
            << (g_font_tile ? "OK" : "NULL")
            << " clock=" << (g_font_clock ? "OK" : "NULL") << " atlas=" << width
            << "x" << height << "\n";
}

std::string upper(std::string s) {
  for (char &c : s)
    c = (char)std::toupper((unsigned char)c);
  return s;
}

ImU32 colScaled(const TileColor &c, float f, int a) {
  return IM_COL32((int)std::clamp(c.r * f, 0.0f, 255.0f),
                  (int)std::clamp(c.g * f, 0.0f, 255.0f),
                  (int)std::clamp(c.b * f, 0.0f, 255.0f), a);
}

float flerp(float a, float b, float t) { return a + (b - a) * t; }
float smooth(float t) { return t * t * (3.0f - 2.0f * t); }

} // namespace ui