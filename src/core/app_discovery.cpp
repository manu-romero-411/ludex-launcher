#include "app_discovery.h"

#include "app_backends.h"
#include <algorithm>
#include <cctype>
#include <fstream>
#include <iostream>
#include <sstream>

#include "ini.h"
#include "util.h"

static std::filesystem::path resolveIcon(const Config &cfg,
                                         const std::string &icon_key,
                                         const std::string &stem) {
  std::error_code ec;

  // 1) Ruta exacta de la clave icon= (si existe de verdad)
  if (!icon_key.empty()) {
    std::filesystem::path p = cfg.apps_dir / icon_key;
    if (std::filesystem::is_regular_file(p, ec))
      return p;
  }

  // 2) Fallback: app-icons/<stem> con cada extensión soportada
  for (const auto &ext : cfg.icon_exts) {
    std::filesystem::path p = cfg.apps_dir / "app-icons" / (stem + ext);
    if (std::filesystem::is_regular_file(p, ec))
      return p;
  }

  // 3) Nada encontrado: aviso UNA vez en discovery (no N en carga)
  if (!icon_key.empty()) {
    std::cerr << "[ludex] icono no encontrado para '" << stem
              << "' (buscado: " << icon_key << " y fallbacks)\n";
  }

  return {}; // icon_path vacío -> no se intenta cargar, sin spam
}

static std::string displayNameFromStem(std::string stem) {
  for (char &c : stem) {
    if (c == '_' || c == '-')
      c = ' ';
  }
  bool cap = true;
  for (char &c : stem) {
    unsigned char uc = static_cast<unsigned char>(c);
    if (std::isspace(uc)) {
      cap = true;
    } else {
      c = cap ? (char)std::toupper(uc) : (char)std::tolower(uc);
      cap = false;
    }
  }
  return stem;
}

static bool parseHexColor(const std::string &s, TileColor &out) {
  std::string t = util::trim(s);
  if (!t.empty() && t[0] == '#')
    t = t.substr(1);
  if (t.size() != 6)
    return false;

  auto hex2 = [](const std::string &v, size_t off) -> int {
    return (int)std::stoul(v.substr(off, 2), nullptr, 16);
  };

  out.r = (unsigned char)hex2(t, 0);
  out.g = (unsigned char)hex2(t, 2);
  out.b = (unsigned char)hex2(t, 4);
  return true;
}

static App parseWebapp(const std::filesystem::path &path, const Config &cfg,
                       const BackendRegistry &backends, bool &ok) {
  ok = true;
  App app;
  app.name = displayNameFromStem(path.stem().string());
  app.webapp_path = path;

  std::ifstream f(path);
  std::stringstream buf;
  buf << f.rdbuf();
  std::string content = buf.str();

  if (content.find("[ludex-element]") != std::string::npos) {
    Ini ini;
    ini.load(path);
    const std::string S = "ludex-element";

    app.name = ini.get(S, "name", app.name);

    // backend= (preferida) > app= > type= (legacy)
    app.backend = ini.get(S, "backend", "");

    app.run = ini.get(S, "run", "");

    std::string icon_key = ini.get(S, "icon");
    app.icon_path = resolveIcon(cfg, icon_key, path.stem().string());

    std::string tt = ini.get(S, "tile_type", "flat");
    app.tile_type = (tt == "gradient2") ? 1 : 0;

    parseHexColor(ini.get(S, "color1"), app.color1);
    parseHexColor(ini.get(S, "color2"), app.color2);

    std::string tint_str = ini.get(S, "icon_color");
    if (tint_str.empty())
      tint_str = ini.get(S, "tint");
    if (!tint_str.empty() && parseHexColor(tint_str, app.icon_tint)) {
      app.has_icon_tint = true;
    }
    // read text_color
    std::string text_color_str = ini.get(S, "text_color");
    if (!text_color_str.empty() &&
        parseHexColor(text_color_str, app.text_color)) {
      app.has_text_color = true;
    }

    //  priority, order_name, hidden
    app.priority = ini.getInt(S, "priority", 0);
    app.order_name = ini.get(S, "order_name", "");
    app.hidden = ini.getBool(S, "hidden", false);

  } else {
    // old format: only url -> backend "webapp"
    app.backend = "webapp";
    app.run = util::trim(content);
    app.icon_path = resolveIcon(cfg, "", path.stem().string());
  }

  // Si el backend no existe, la app NO aparece en el menú
  if (app.backend.empty() || !backends.find(app.backend)) {
    std::cerr << "[ludex] '" << path.filename().string()
              << "' referencia el backend '" << app.backend
              << "' que no está definido; se excluye del menú\n";
    ok = false;
  }

  return app;
}

std::vector<App> discoverApps(const Config &cfg,
                              const BackendRegistry &backends) {
  std::vector<App> apps;

  std::error_code ec;
  if (!std::filesystem::is_directory(cfg.apps_dir, ec)) {
    std::cerr << "[ludex] APPS_DIR no existe: " << cfg.apps_dir << "\n";
    return apps;
  }

  std::vector<std::filesystem::path> entries;
  for (const auto &e : std::filesystem::directory_iterator(cfg.apps_dir, ec)) {
    if (e.is_regular_file(ec) && e.path().extension() == ".webapp") {
      entries.push_back(e.path());
    }
  }
  std::sort(entries.begin(), entries.end());

  for (const auto &p : entries) {
    bool ok = false;
    App app = parseWebapp(p, cfg, backends, ok);
    if (ok && !app.hidden)
      apps.push_back(std::move(app));
  }
  // Orden deseado:
  // 1) Apps con priority primero, ordenadas por priority ascendente.
  // 2) Si no hay priority o hay empate, usar order_name.
  //    Las apps con order_name van antes que las que no tienen.
  // 3) Si no hay order_name o hay empate, usar nombre de archivo.
  std::sort(apps.begin(), apps.end(), [](const App &a, const App &b) {
    const bool a_has_priority = a.priority != 0;
    const bool b_has_priority = b.priority != 0;

    // Una app con priority va antes que una sin priority.
    if (a_has_priority != b_has_priority)
      return a_has_priority;

    // Si ambas tienen priority, ordenar por priority.
    if (a_has_priority && a.priority != b.priority)
      return a.priority < b.priority;

    const bool a_has_order_name = !a.order_name.empty();
    const bool b_has_order_name = !b.order_name.empty();

    // Una app con order_name va antes que una sin order_name.
    if (a_has_order_name != b_has_order_name)
      return a_has_order_name;

    // Si ambas tienen order_name, ordenar alfabéticamente.
    if (a_has_order_name && a.order_name != b.order_name)
      return a.order_name < b.order_name;

    // Desempate final: nombre de archivo.
    return a.webapp_path.filename().string() <
           b.webapp_path.filename().string();
  });

  return apps;
}