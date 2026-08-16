#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "config.h"
class BackendRegistry;

struct TileColor {
  unsigned char r = 40, g = 40, b = 46;
};

struct App {
  std::string name;       // nombre mostrado (INI name= o filename stem)
  std::string order_name; // para ordenar (si no existe, usa name)
  int priority = 0;       // menor = antes; 0 = sin prioridad
  bool hidden = false;    // hidden=true → oculta del carrusel
  std::string backend;
  std::string run;
  std::filesystem::path webapp_path;
  std::filesystem::path icon_path;
  TileColor icon_tint{255, 255, 255};
  bool has_icon_tint = false;
  TileColor text_color{255, 255, 255};
  bool has_text_color = false;
  int tile_type = 0;
  TileColor color1;
  TileColor color2;
};
std::vector<App> discoverApps(const Config &cfg,
                              const BackendRegistry &backends);