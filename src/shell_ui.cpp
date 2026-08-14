#include "shell_ui.h"

#include "imgui_internal.h"
#include "input_manager.h"
#include <imgui.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

/* ================================================================
 * Fuentes y utilidades
 * ================================================================ */

static ImFont *g_font_tile = nullptr;
static ImFont *g_font_clock = nullptr;
static ImFont *g_font_date = nullptr;
static ImFont *g_font_hint = nullptr;

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

static std::string upper(std::string s) {
  for (char &c : s)
    c = (char)std::toupper((unsigned char)c);
  return s;
}

static ImU32 colRGBA(int r, int g, int b, int a = 255) {
  return IM_COL32(r, g, b, a);
}

static ImU32 colScaled(const TileColor &c, float f, int a = 255) {
  return IM_COL32((int)std::clamp(c.r * f, 0.0f, 255.0f),
                  (int)std::clamp(c.g * f, 0.0f, 255.0f),
                  (int)std::clamp(c.b * f, 0.0f, 255.0f), a);
}

static float flerp(float a, float b, float t) { return a + (b - a) * t; }
static float smooth(float t) { return t * t * (3.0f - 2.0f * t); }

static std::string fmt3(float v) {
  char b[32];
  std::snprintf(b, sizeof(b), "%.3f", v);
  return b;
}

static void *uiIcon(const UiIcons &ic, int idx) {
  switch (idx) {
  case 1:
    return ic.settings;
  case 2:
    return ic.exit;
  case 3:
    return ic.shutdown;
  case 4:
    return ic.restart;
  case 5:
    return ic.suspend;
  default:
    return nullptr;
  }
}

static std::string
assignmentLabel(const Config &cfg,
                const std::vector<InputManager::DeviceInfo> &devs, int p) {
  if (cfg.controller_guid[p].empty())
    return "DEFAULT";
  for (const auto &d : devs)
    if (d.guid == cfg.controller_guid[p])
      return "#" + std::to_string(d.sdl_index) + " " + d.name;
  return "OFFLINE (" + cfg.controller_name[p] + ")";
}

void controllersInput(ShellState &st, Config &cfg, const ShellActions &actions,
                      UiAction a) {
  if (!st.show_controllers)
    return;
  auto devs = actions.devices ? actions.devices()
                              : std::vector<InputManager::DeviceInfo>{};

  if (st.controller_pick_player < 0) {
    const int count = 9; // 8 jugadores + BACK
    int &f = st.controllers_focus;
    switch (a) {
    case UiAction::Up:
      f = (f - 1 + count) % count;
      break;
    case UiAction::Down:
      f = (f + 1) % count;
      break;
    case UiAction::Select:
      if (f < 8) {
        st.controller_pick_player = f;
        st.controller_pick_focus = 0;
      } else
        st.show_controllers = false;
      break;
    case UiAction::Back:
    case UiAction::Guide:
      st.show_controllers = false;
      break;
    default:
      break;
    }
  } else {
    const int rows = 1 + (int)devs.size();
    const int count = rows + 1;
    int &f = st.controller_pick_focus;
    switch (a) {
    case UiAction::Up:
      f = (f - 1 + count) % count;
      break;
    case UiAction::Down:
      f = (f + 1) % count;
      break;
    case UiAction::Select:
      if (f < rows) {
        int p = st.controller_pick_player;
        if (f == 0) {
          cfg.controller_guid[p].clear();
          cfg.controller_name[p].clear();
        } else {
          cfg.controller_guid[p] = devs[f - 1].guid;
          cfg.controller_name[p] = devs[f - 1].name;
        }
        if (actions.apply_controllers)
          actions.apply_controllers();
        cfg.save(cfg.ini_path);
        st.controller_pick_player = -1;
      } else
        st.controller_pick_player = -1;
      break;
    case UiAction::Back:
    case UiAction::Guide:
      st.controller_pick_player = -1;
      break;
    default:
      break;
    }
  }
}

/* ================================================================
 * Elementos fijos: reloj, fades de borde, menú pequeño de abajo, hints
 * ================================================================ */

static void drawClock(const Config &cfg, const ImGuiViewport *vp,
                      bool left_side, bool bottom) {
  std::time_t now = std::time(nullptr);
  std::tm *local = std::localtime(&now);

  char time_text[32], date_text[128];
  std::strftime(time_text, sizeof(time_text), "%H:%M:%S", local);
  std::strftime(date_text, sizeof(date_text), "%A %d de %B de %Y", local);

  float margin = vp->WorkSize.x * 0.018f;

  ImGui::PushFont(g_font_clock);
  ImVec2 ts = ImGui::CalcTextSize(time_text);
  ImGui::PopFont();
  ImGui::PushFont(g_font_date);
  ImVec2 ds = ImGui::CalcTextSize(date_text);
  ImGui::PopFont();

  float x_time = left_side ? vp->WorkSize.x - margin - ts.x : margin;
  float x_date = left_side ? vp->WorkSize.x - margin - ds.x : margin;

  float y_time, y_date;
  if (bottom) {
    y_date = vp->WorkSize.y - margin - ds.y;
    y_time = y_date - 6.0f - ts.y;
  } else {
    y_time = margin;
    y_date = margin + ts.y + 6.0f;
  }

  ImGui::PushFont(g_font_clock);
  ImGui::SetCursorScreenPos(ImVec2(x_time, y_time));
  ImGui::TextUnformatted(time_text);
  ImGui::PopFont();

  ImGui::PushFont(g_font_date);
  ImGui::SetCursorScreenPos(ImVec2(x_date, y_date));
  ImGui::TextColored(ImVec4(0.85f, 0.85f, 0.88f, 0.9f), "%s", date_text);
  ImGui::PopFont();
}

static void drawEdgeFades(const Config &cfg, float W, float H) {
  ImDrawList *dl = ImGui::GetWindowDrawList();
  int a = (int)std::clamp(cfg.edge_fade_alpha, 0.0f, 255.0f);
  bool light = (cfg.theme == "light");

  ImU32 edge = light ? IM_COL32(245, 245, 245, a) : IM_COL32(0, 0, 0, a);
  ImU32 none = light ? IM_COL32(245, 245, 245, 0) : IM_COL32(0, 0, 0, 0);

  float left_fw = W * (cfg.tile_sel_w_pct + cfg.edge_fade_pct);
  dl->AddRectFilledMultiColor(ImVec2(0.0f, 0.0f), ImVec2(left_fw, H), edge,
                              none, none, edge);

  float right_fw = W * cfg.edge_fade_pct;
  dl->AddRectFilledMultiColor(ImVec2(W - right_fw, 0.0f), ImVec2(W, H), none,
                              edge, edge, none);
}

static void drawSystemMenu(const ShellState &state, const Config &cfg,
                           const ShellActions &actions, float W, float H) {
  if (state.menu_anim <= 0.001f)
    return;

  ImDrawList *dl = ImGui::GetWindowDrawList();
  bool left = (cfg.side == "left" || cfg.side == "bottom");
  bool light = (cfg.theme == "light");

  ImU32 row_normal =
      light ? IM_COL32(230, 232, 240, 255) : IM_COL32(32, 32, 36, 255);
  ImU32 row_focus =
      light ? IM_COL32(200, 210, 230, 255) : IM_COL32(74, 74, 80, 255);
  ImU32 text_col =
      light ? IM_COL32(25, 25, 30, 255) : IM_COL32(230, 230, 230, 255);

  float menu_h = H * cfg.menu_h_pct;
  float menu_w = W * cfg.tile_sel_w_pct;
  float total = menu_h * 4.0f;
  float y_base = H - total * state.menu_anim;

  static const char *labels[4] = {"SETTINGS", "CONTROLLERS", "QUIT TO DESKTOP",
                                  "SHUTDOWN..."};
  void *icons[4] = {state.ui_icons.settings, state.ui_icons.gamepad,
                    state.ui_icons.exit, state.ui_icons.shutdown};

  for (int m = 0; m < 4; ++m) {
    float y0 = y_base + m * menu_h;
    ImVec2 min(left ? 0.0f : W - menu_w, y0);
    ImVec2 max(min.x + menu_w, y0 + menu_h);

    ImU32 col = (state.menu_selected == m) ? row_focus : row_normal;
    dl->AddRectFilled(min, max, col);

    if (state.menu_open) {
      ImGui::PushID(500 + m);
      ImGui::SetCursorScreenPos(min);
      ImGui::InvisibleButton("##sys", ImVec2(menu_w, menu_h));
      if (ImGui::IsItemClicked()) {
        if (m == 0)
          actions.open_settings();
        else if (m == 1) {
          if (actions.open_controllers)
            actions.open_controllers();
        } else if (m == 2)
          actions.quit();
        else
          actions.poweroff();
      }
      ImGui::PopID();
    }

    std::string label = labels[m];
    ImGui::PushFont(g_font_tile);
    ImVec2 ts = ImGui::CalcTextSize(label.c_str());

    float isz = menu_h * 0.55f;
    float lx = min.x + W * 0.012f;
    if (icons[m]) {
      dl->AddImage((ImTextureID)icons[m],
                   ImVec2(lx, y0 + (menu_h - isz) * 0.5f),
                   ImVec2(lx + isz, y0 + (menu_h + isz) * 0.5f), ImVec2(0, 0),
                   ImVec2(1, 1), text_col);
      lx += isz + W * 0.010f;
    }
    dl->AddText(ImVec2(lx, y0 + (menu_h - ts.y) * 0.5f), text_col,
                label.c_str());
    ImGui::PopFont();
  }
}

static void drawHints(const ShellState &state, const Config &cfg,
                      const ImGuiViewport *vp) {
  float W = vp->WorkSize.x, H = vp->WorkSize.y;
  ImDrawList *dl = ImGui::GetWindowDrawList();
  bool light = (cfg.theme == "light");
  ImU32 col = light ? IM_COL32(40, 40, 45, 220) : IM_COL32(235, 235, 235, 220);
  const UiIcons &ic = state.ui_icons;

  struct Seg {
    void *tex;
    const char *fallback;
    const char *label;
  };
  bool horiz = (cfg.side == "top" || cfg.side == "bottom");

  std::vector<Seg> segs;
  if (state.show_settings) {
    segs = {{ic.nav_v, "UP/DOWN", "NAVIGATE"},
            {ic.nav_h, "LEFT/RIGHT", "CHANGE"},
            {ic.accept, "A", "OK"},
            {ic.back, "B", "BACK"}};
  } else if (state.show_settings || state.show_controllers) {
    segs = {{ic.nav_v, "UP/DOWN", "NAVIGATE"},
            {ic.nav_h, "LEFT/RIGHT", "CHANGE"},
            {ic.accept, "A", "OK"},
            {ic.back, "B", "BACK"}};
  } else if (state.show_power) {
    segs = {{ic.nav_v, "UP/DOWN", "NAVIGATE"},
            {ic.accept, "A", "ACCEPT"},
            {ic.back, "B", "BACK"}};
  } else if (state.menu_open) {
    segs = {{ic.nav_v, "UP/DOWN", "NAVIGATE"},
            {ic.accept, "A", "ACCEPT"},
            {ic.back, "B / HOME", "CLOSE"}};

  } else {
    segs = {{horiz ? ic.nav_h : ic.nav_v, horiz ? "LEFT/RIGHT" : "UP/DOWN",
             "NAVIGATE"},
            {ic.accept, "A", "SELECT"},
            {ic.back, "B", "BACK"},
            {ic.home, "HOME", "SYSTEM MENU"}};
  }

  ImGui::PushFont(g_font_hint);

  float gap_seg = W * 0.014f;
  float gap_it = W * 0.004f;

  struct M {
    std::string text;
    float tw, iw;
    ImVec2 ts;
  };
  std::vector<M> ms;
  float total = 0, base_h = 0;
  for (auto &s : segs) {
    M m;
    m.text = s.tex ? s.label : (std::string(s.fallback) + " " + s.label);
    m.ts = ImGui::CalcTextSize(m.text.c_str());
    m.tw = m.ts.x;
    m.iw = s.tex ? m.ts.y * 1.25f : 0.0f;
    base_h = std::max(base_h, m.ts.y);
    total += m.tw + (s.tex ? m.iw + gap_it : 0.0f);
    ms.push_back(m);
  }
  total += gap_seg * (float)(segs.size() - 1);

  float margin = H * 0.008f;               // más pegada al borde
  bool hints_top = (cfg.side == "bottom"); // en modo bottom, arriba
  float y = hints_top ? margin : (H - margin - base_h);
  float x = (W - total) * 0.5f;

  for (size_t i = 0; i < segs.size(); ++i) {
    const auto &s = segs[i];
    const auto &m = ms[i];
    float cx = x;

    if (s.tex) {
      float hs = base_h * 1.25f;
      float iy = y + (base_h - hs) * 0.5f;
      dl->AddImage((ImTextureID)s.tex, ImVec2(cx, iy), ImVec2(cx + hs, iy + hs),
                   ImVec2(0, 0), ImVec2(1, 1),
                   IM_COL32(255, 255, 255, 220)); // <-- sin tinte de tema
      cx += hs + gap_it;
    }

    dl->AddText(ImVec2(cx, y), col, m.text.c_str());
    x += m.tw + (s.tex ? m.iw + gap_it : 0.0f) + gap_seg;
  }
  ImGui::PopFont();
}

/* ================================================================
 * Paneles estilo EmulationStation (settings + power)
 * ================================================================ */

namespace {

struct SRow {
  const char *label;
  bool adjustable;
  std::string (*get)(const Config &);
  void (*adjust)(Config &, int dir);
  void (*activate)(ShellState &, Config &, const ShellActions &);
  int icon; // 0=ninguno 1=settings 2=exit 3=shutdown 4=restart 5=suspend
};

const SRow kSettingsRows[] = {
    {"APP DRAWER MODE", true, [](const Config &c) { return c.side; },
     [](Config &c, int) {
       if (c.side == "left") c.side = "right";
       else if (c.side == "right") c.side = "top";
       else if (c.side == "top") c.side = "bottom";
       else c.side = "left";
     }, nullptr, 0},
    {"# VISIBLE ELEMENTS", true,
     [](const Config &c) { return std::to_string(c.visible_items); },
     [](Config &c, int d) {
       c.visible_items = std::clamp(c.visible_items + 2 * d, 5, 11);
       if (c.visible_items % 2 == 0) c.visible_items--;
     }, nullptr, 0},
    {"SHOW GAMEPAD INDICATORS", true,
     [](const Config &c) -> std::string {
       return c.show_player_indicators ? "YES" : "NO";
     },
     [](Config &c, int) { c.show_player_indicators = !c.show_player_indicators; },
     nullptr, 0},
    {"COLOUR SCHEME", true, [](const Config &c) { return c.theme; },
     [](Config &c, int) { c.theme = (c.theme == "dark") ? "light" : "dark"; },
     nullptr, 0},
    {"HELP ICONS", false, [](const Config &c) { return c.help_icons; }, nullptr,
     [](ShellState &, Config &c, const ShellActions &a) {
       c.help_icons = (c.help_icons == "xbox") ? "playstation"
                      : (c.help_icons == "playstation") ? "none" : "xbox";
       if (a.reload_ui_icons) a.reload_ui_icons();
     }, 0},
    {"ALL PLAYERS CONTROL UI", true,
     [](const Config &c) -> std::string { return c.all_players_ui ? "YES" : "NO"; },
     [](Config &c, int) { c.all_players_ui = !c.all_players_ui; }, nullptr, 0},
    {"GO BACK", false, nullptr, nullptr,
     [](ShellState &s, Config &c, const ShellActions &) {
       c.save(c.ini_path);
       s.show_settings = false;
     }, 0},
};

const SRow kPowerRows[] = {
    {"SLEEP", false, nullptr, nullptr,
     [](ShellState &s, Config &, const ShellActions &a) {
       if (a.suspend)
         a.suspend();
       s.show_power = false;
     },
     5},
    {"REBOOT", false, nullptr, nullptr,
     [](ShellState &s, Config &, const ShellActions &a) {
       if (a.reboot)
         a.reboot();
       s.show_power = false;
     },
     4},
    {"POWER OFF", false, nullptr, nullptr,
     [](ShellState &s, Config &, const ShellActions &a) {
       if (a.poweroff)
         a.poweroff();
       s.show_power = false;
     },
     3},
    {"VOLVER", false, nullptr, nullptr,
     [](ShellState &s, Config &, const ShellActions &) {
       s.show_power = false;
     },
     2},
};

constexpr int kSettingsCount =
    (int)(sizeof(kSettingsRows) / sizeof(kSettingsRows[0]));
constexpr int kPowerCount = (int)(sizeof(kPowerRows) / sizeof(kPowerRows[0]));

} // namespace

void panelInput(ShellState &st, Config &cfg, const ShellActions &actions,
                UiAction a) {
  bool settings = st.show_settings;
  if (!settings && !st.show_power)
    return;

  const SRow *rows = settings ? kSettingsRows : kPowerRows;
  int count = settings ? kSettingsCount : kPowerCount;
  int *pf = settings ? &st.settings_focus : &st.power_focus;

  switch (a) {
  case UiAction::Up:
    *pf = (*pf - 1 + count) % count;
    break;
  case UiAction::Down:
    *pf = (*pf + 1) % count;
    break;
  case UiAction::Left:
    if (rows[*pf].adjust)
      rows[*pf].adjust(cfg, -1);
    break;
  case UiAction::Right:
    if (rows[*pf].adjust)
      rows[*pf].adjust(cfg, +1);
    break;
  case UiAction::Select:
    if (rows[*pf].activate)
      rows[*pf].activate(st, cfg, actions);
    else if (rows[*pf].adjust)
      rows[*pf].adjust(cfg, +1);
    break;
  case UiAction::Back:
  case UiAction::Guide:
    if (settings) {
      cfg.save(cfg.ini_path);
      st.show_settings = false;
    } else {
      st.show_power = false;
    }
    break;
  default:
    break;
  }
}

static void drawPanel(ShellState &st, Config &cfg, const ShellActions &actions,
                      bool settings) {
  const ImGuiViewport *vp = ImGui::GetMainViewport();
  float W = vp->WorkSize.x, H = vp->WorkSize.y;
  ImDrawList *dl = ImGui::GetWindowDrawList();

  // BLOQUEAR clicks de fondo
  ImGui::SetCursorScreenPos(ImVec2(0, 0));
  ImGui::InvisibleButton("##panel_bg", ImVec2(W, H));

  bool light = (cfg.theme == "light");

  ImU32 fade_col =
      light ? IM_COL32(240, 240, 240, 210) : IM_COL32(0, 0, 0, 210);
  ImU32 panel_bg =
      light ? IM_COL32(250, 250, 252, 250) : IM_COL32(30, 32, 40, 250);
  ImU32 panel_title =
      light ? IM_COL32(230, 232, 240, 255) : IM_COL32(44, 47, 60, 255);
  ImU32 row_focus =
      light ? IM_COL32(210, 220, 240, 255) : IM_COL32(78, 82, 98, 255);
  ImU32 text_main =
      light ? IM_COL32(25, 25, 30, 255) : IM_COL32(230, 230, 230, 255);
  ImU32 text_val =
      light ? IM_COL32(40, 40, 50, 255) : IM_COL32(240, 240, 240, 255);
  ImU32 border_col =
      light ? IM_COL32(120, 125, 140, 255) : IM_COL32(150, 155, 170, 200);

  dl->AddRectFilled(ImVec2(0, 0), ImVec2(W, H), fade_col);

  const SRow *rows = settings ? kSettingsRows : kPowerRows;
  int count = settings ? kSettingsCount : kPowerCount;
  int focus = settings ? st.settings_focus : st.power_focus;
  const char *title = settings ? "SETTINGS" : "SHUTDOWN...";

  int footer_rows = 1; // GUARDAR+VOLVER / VOLVER
  int list_count = count - footer_rows;

  float row_h = H * 0.055f;
  float title_h = H * 0.09f;
  float footer_h = row_h * 1.6f;
  float pw = W * 0.56f;
  float ph = title_h + list_count * row_h + footer_h;
  float px = (W - pw) * 0.5f;
  float py = (H - ph) * 0.5f;
  float pad = W * 0.012f;

  dl->AddRectFilled(ImVec2(px, py), ImVec2(px + pw, py + ph), panel_bg, 10.0f);
  dl->AddRectFilled(ImVec2(px, py), ImVec2(px + pw, py + title_h), panel_title,
                    10.0f, ImDrawFlags_RoundCornersTop);

  ImGui::PushFont(g_font_tile);
  ImVec2 tt = ImGui::CalcTextSize(title);
  dl->AddText(ImVec2(px + (pw - tt.x) * 0.5f, py + (title_h - tt.y) * 0.5f),
              text_main, title);

  /* ---- filas de la lista ---- */
  for (int i = 0; i < list_count; ++i) {
    float y0 = py + title_h + i * row_h;
    ImVec2 rmin(px, y0), rmax(px + pw, y0 + row_h);

    if (i == focus)
      dl->AddRectFilled(rmin, rmax, row_focus);

    ImGui::PushID(9000 + i);
    ImGui::SetCursorScreenPos(rmin);
    ImGui::InvisibleButton("##row", ImVec2(pw, row_h));
    if (ImGui::IsItemClicked()) {
      (settings ? st.settings_focus : st.power_focus) = i;
      if (rows[i].activate)
        rows[i].activate(st, cfg, actions);
      else if (rows[i].adjust)
        rows[i].adjust(cfg, +1);
    }
    ImGui::PopID();

    void *ricon = uiIcon(st.ui_icons, rows[i].icon);
    float isz = row_h * 0.6f;
    float lx = px + pad;
    if (ricon) {
      dl->AddImage((ImTextureID)ricon, ImVec2(lx, y0 + (row_h - isz) * 0.5f),
                   ImVec2(lx + isz, y0 + (row_h + isz) * 0.5f), ImVec2(0, 0),
                   ImVec2(1, 1), text_main);
      lx += isz + pad * 0.8f;
    }
    dl->AddText(ImVec2(lx, y0 + (row_h - tt.y) * 0.5f), text_main,
                rows[i].label);

    if (rows[i].get) {
      std::string val = rows[i].get(cfg);
      if (rows[i].adjustable)
        val = "<  " + val + "  >";
      ImVec2 vs = ImGui::CalcTextSize(val.c_str());
      dl->AddText(ImVec2(px + pw - pad - vs.x, y0 + (row_h - vs.y) * 0.5f),
                  text_val, val.c_str());
    }
  }

  /* ---- botones tipo ES en el pie, sin icono ---- */
  float bh = row_h * 0.95f;
  float bw = pw * (footer_rows == 2 ? 0.30f : 0.24f);
  float gapb = pw * 0.04f;
  float total_w = footer_rows * bw + (footer_rows - 1) * gapb;
  float bx0 = px + (pw - total_w) * 0.5f;
  float by = py + title_h + list_count * row_h + (footer_h - bh) * 0.5f;

  for (int f = 0; f < footer_rows; ++f) {
    int i = list_count + f;
    ImVec2 bmin(bx0 + f * (bw + gapb), by);
    ImVec2 bmax(bmin.x + bw, bmin.y + bh);

    if (focus == i)
      dl->AddRectFilled(bmin, bmax, row_focus, 6.0f);
    dl->AddRect(bmin, bmax, border_col, 6.0f, 0, 2.0f);

    const char *blabel = rows[i].label;
    ImVec2 bs = ImGui::CalcTextSize(blabel);
    dl->AddText(
        ImVec2(bmin.x + (bw - bs.x) * 0.5f, bmin.y + (bh - bs.y) * 0.5f),
        text_main, blabel);

    ImGui::PushID(9100 + i);
    ImGui::SetCursorScreenPos(bmin);
    ImGui::InvisibleButton("##fbtn", ImVec2(bw, bh));
    if (ImGui::IsItemClicked()) {
      (settings ? st.settings_focus : st.power_focus) = i;
      if (rows[i].activate)
        rows[i].activate(st, cfg, actions);
    }
    ImGui::PopID();
  }

  ImGui::PopFont();
}

static void drawControllersPanel(ShellState &st, Config &cfg,
                                 const ShellActions &actions) {
  const ImGuiViewport *vp = ImGui::GetMainViewport();
  float W = vp->WorkSize.x, H = vp->WorkSize.y;
  ImDrawList *dl = ImGui::GetWindowDrawList();

  // BLOQUEAR clicks de fondo
  ImGui::SetCursorScreenPos(ImVec2(0, 0));
  ImGui::InvisibleButton("##controllers_bg", ImVec2(W, H));

  bool light = cfg.theme == "light";

  ImU32 fade_col =
      light ? IM_COL32(240, 240, 240, 210) : IM_COL32(0, 0, 0, 210);
  ImU32 panel_bg =
      light ? IM_COL32(250, 250, 252, 250) : IM_COL32(30, 32, 40, 250);
  ImU32 panel_title =
      light ? IM_COL32(230, 232, 240, 255) : IM_COL32(44, 47, 60, 255);
  ImU32 row_focus =
      light ? IM_COL32(210, 220, 240, 255) : IM_COL32(78, 82, 98, 255);
  ImU32 text_main =
      light ? IM_COL32(25, 25, 30, 255) : IM_COL32(230, 230, 230, 255);
  ImU32 text_val =
      light ? IM_COL32(40, 40, 50, 255) : IM_COL32(240, 240, 240, 255);
  ImU32 border_col =
      light ? IM_COL32(120, 125, 140, 255) : IM_COL32(150, 155, 170, 200);

  dl->AddRectFilled(ImVec2(0, 0), ImVec2(W, H), fade_col);

  auto devs = actions.devices ? actions.devices()
                              : std::vector<InputManager::DeviceInfo>{};
  bool picking = st.controller_pick_player >= 0;

  std::vector<std::pair<std::string, std::string>> rows;
  std::string title;
  if (!picking) {
    title = "CONTROLLER ASSIGNMENT";
    for (int p = 0; p < 8; ++p)
      rows.push_back({"PLAYER " + std::to_string(p + 1) + " PAD",
                      assignmentLabel(cfg, devs, p)});
  } else {
    title = "PAD FOR PLAYER " + std::to_string(st.controller_pick_player + 1);
    rows.push_back({"DEFAULT", ""});
    for (const auto &d : devs)
      rows.push_back({"#" + std::to_string(d.sdl_index) + " " + d.name, ""});
  }

  int list_count = (int)rows.size();
  int focus = picking ? st.controller_pick_focus : st.controllers_focus;

  float row_h = H * 0.055f, title_h = H * 0.09f, footer_h = row_h * 1.6f;
  float pw = W * 0.56f;
  float ph = title_h + list_count * row_h + footer_h;
  float px = (W - pw) * 0.5f, py = (H - ph) * 0.5f, pad = W * 0.012f;

  dl->AddRectFilled(ImVec2(px, py), ImVec2(px + pw, py + ph), panel_bg, 10.0f);
  dl->AddRectFilled(ImVec2(px, py), ImVec2(px + pw, py + title_h), panel_title,
                    10.0f, ImDrawFlags_RoundCornersTop);

  ImGui::PushFont(g_font_tile);
  ImVec2 tt = ImGui::CalcTextSize(title.c_str());
  dl->AddText(ImVec2(px + (pw - tt.x) * 0.5f, py + (title_h - tt.y) * 0.5f),
              text_main, title.c_str());

  for (int i = 0; i < list_count; ++i) {
    float y0 = py + title_h + i * row_h;
    if (i == focus)
      dl->AddRectFilled(ImVec2(px, y0), ImVec2(px + pw, y0 + row_h), row_focus);

    ImGui::PushID(9300 + i);
    ImGui::SetCursorScreenPos(ImVec2(px, y0));
    ImGui::InvisibleButton("##crow", ImVec2(pw, row_h));
    if (ImGui::IsItemClicked()) {
      if (picking)
        st.controller_pick_focus = i;
      else
        st.controllers_focus = i;
      controllersInput(st, cfg, actions, UiAction::Select);
    }
    ImGui::PopID();

    dl->AddText(ImVec2(px + pad, y0 + (row_h - tt.y) * 0.5f), text_main,
                rows[i].first.c_str());
    if (!rows[i].second.empty()) {
      ImVec2 vs = ImGui::CalcTextSize(rows[i].second.c_str());
      dl->AddText(ImVec2(px + pw - pad - vs.x, y0 + (row_h - vs.y) * 0.5f),
                  text_val, rows[i].second.c_str());
    }
  }

  // BACK
  float bh = row_h * 0.95f, bw = pw * 0.24f;
  ImVec2 bmin(px + (pw - bw) * 0.5f,
              py + title_h + list_count * row_h + (footer_h - bh) * 0.5f);
  ImVec2 bmax(bmin.x + bw, bmin.y + bh);
  if (focus == list_count)
    dl->AddRectFilled(bmin, bmax, row_focus, 6);
  dl->AddRect(bmin, bmax, border_col, 6, 0, 2);
  ImVec2 bs = ImGui::CalcTextSize("BACK");
  dl->AddText(ImVec2(bmin.x + (bw - bs.x) * 0.5f, bmin.y + (bh - bs.y) * 0.5f),
              text_main, "BACK");
  ImGui::PushID(9400);
  ImGui::SetCursorScreenPos(bmin);
  ImGui::InvisibleButton("##cback", ImVec2(bw, bh));
  if (ImGui::IsItemClicked()) {
    if (picking)
      st.controller_pick_player = -1;
    else
      st.show_controllers = false;
  }
  ImGui::PopID();
  ImGui::PopFont();
}

static void drawWallpaperLayer(ImDrawList *dl, float W, float H,
                               const WallpaperLayer &L, ImU32 tint) {
  if (!L.texture || L.w <= 0 || L.h <= 0)
    return;

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

  dl->AddImage((ImTextureID)L.texture, ImVec2(0, 0), ImVec2(W, H), uv0, uv1,
               tint);
}

/* Silueta de mando de reserva (si no existe gamepad.svg) */
static void drawGamepadGlyph(ImDrawList *dl, ImVec2 min, ImVec2 max,
                             ImU32 col) {
  float w = max.x - min.x, h = max.y - min.y;
  ImVec2 bmin(min.x, min.y + h * 0.22f);
  ImVec2 bmax(max.x, max.y - h * 0.10f);
  dl->AddRectFilled(bmin, bmax, col, h * 0.28f);
  dl->AddCircleFilled(ImVec2(min.x + w * 0.20f, min.y + h * 0.28f), h * 0.16f,
                      col);
  dl->AddCircleFilled(ImVec2(max.x - w * 0.20f, min.y + h * 0.28f), h * 0.16f,
                      col);
}

static void
drawPlayerIndicators(const ShellState &state, const Config &cfg,
                     const std::vector<InputManager::PlayerStatus> &players,
                     const ImGuiViewport *vp) {
  if (players.empty())
    return;

  ImDrawList *dl = ImGui::GetWindowDrawList();
  float W = vp->WorkSize.x, H = vp->WorkSize.y;
  bool light = (cfg.theme == "light");

  // Mismo esquema de color que el resto de la UI + verde eléctrico en activo
  ImU32 idle = light ? IM_COL32(40, 40, 45, 220) : IM_COL32(220, 222, 228, 220);
  ImU32 active = IM_COL32(0, 255, 90, 255);

  bool gp_top = (cfg.side == "bottom");
  bool gp_right = (cfg.side != "right");

  float s = H * 0.024f;
  float gap = s * 0.4f;
  float margin = H * 0.018f;
  int n = std::min((int)players.size(), 8); // máximo 8 mandos

  float total = n * s + (n - 1) * gap;
  float x_start = gp_right ? (W - margin - total) : margin;
  float y = gp_top ? margin : (H - margin - s);

  for (int i = 0; i < n; ++i) {
    ImU32 col = players[i].active ? active : idle;
    float x = x_start + i * (s + gap);
    if (state.ui_icons.gamepad) {
      dl->AddImage((ImTextureID)state.ui_icons.gamepad, ImVec2(x, y),
                   ImVec2(x + s, y + s), ImVec2(0, 0), ImVec2(1, 1), col);
    } else {
      drawGamepadGlyph(dl, ImVec2(x, y), ImVec2(x + s, y + s), col);
    }
  }
}

/* ================================================================
 * Orquestación principal
 * ================================================================ */

void drawShellImGui(ShellState &state, const Config &cfg,
                    const ShellActions &actions) {
  const ImGuiViewport *vp = ImGui::GetMainViewport();
  float W = vp->WorkSize.x, H = vp->WorkSize.y;
  bool panel_open =
      state.show_settings || state.show_power || state.show_controllers;

  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
  ImGui::SetNextWindowPos(vp->WorkPos);
  ImGui::SetNextWindowSize(vp->WorkSize);

  ImGuiWindowFlags flags =
      ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground |
      ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoScrollbar |
      ImGuiWindowFlags_NoScrollWithMouse;

  ImGui::Begin("##ludex", nullptr, flags);
  ImDrawList *dl = ImGui::GetWindowDrawList();

  bool has_wp = !state.wallpapers.empty() && state.wp_current >= 0;

  if (has_wp) {
    bool light = (cfg.theme == "light");
    ImU32 overlay_dark = IM_COL32(0, 0, 0, 90);
    ImU32 overlay_light = IM_COL32(255, 255, 255, 80);
    ImU32 overlay = light ? overlay_light : overlay_dark;

    if (state.wp_in_transition && state.wp_next >= 0) {
      float raw = state.wp_fade;
      float f = raw * raw * (3.0f - 2.0f * raw);
      int a_cur = (int)(255 * f);
      int a_nxt = (int)(255 * (1.0f - f));
      ImU32 tint_cur = IM_COL32(255, 255, 255, a_cur);
      ImU32 tint_nxt = IM_COL32(255, 255, 255, a_nxt);

      drawWallpaperLayer(dl, W, H, state.wallpapers[state.wp_current],
                         tint_cur);
      drawWallpaperLayer(dl, W, H, state.wallpapers[state.wp_next], tint_nxt);
    } else {
      drawWallpaperLayer(dl, W, H, state.wallpapers[state.wp_current],
                         IM_COL32(255, 255, 255, 255));
    }
    dl->AddRectFilled(ImVec2(0, 0), ImVec2(W, H), overlay);
  } else {
    bool light = (cfg.theme == "light");
    dl->AddRectFilled(ImVec2(0, 0), ImVec2(W, H),
                      light ? IM_COL32(245, 245, 245, 255)
                            : IM_COL32(18, 18, 20, 255));
  }

  drawEdgeFades(cfg, W, H);

  bool horizontal = (cfg.side == "top" || cfg.side == "bottom");
  bool left = (cfg.side == "left");
  bool top = (cfg.side == "top");

  int V = cfg.visible_items;
  float k = cfg.tile_sel_ratio;
  int N = (int)state.apps.size();

  float main_axis = horizontal ? W : H;
  float cross_axis = horizontal ? H : W;

  float slot = main_axis / ((float)(V - 1) + k);
  float sel_size = slot * k;
  float cross_un = cross_axis * cfg.tile_w_pct;
  float cross_sel = cross_axis * cfg.tile_sel_w_pct;
  float icon_un =
      cross_axis * cfg.icon_pct * (horizontal ? 1.5f : cfg.icon_vert_scale);
  float icon_sel =
      cross_axis * cfg.icon_sel_pct * (horizontal ? 1.5f : cfg.icon_vert_scale);
  float pad = cross_axis * 0.012f;

  auto centerMain = [&](float d) -> float {
    float ad = std::fabs(d);
    float sgn = d >= 0.0f ? 1.0f : -1.0f;
    float s = sgn * smooth(std::min(ad, 1.0f));
    return main_axis * 0.5f + (sel_size - slot) * 0.5f * s + slot * d;
  };

  struct Row {
    int id, idx;
    float d, h, main_pos;
  };
  std::vector<Row> rows;

  const float half = (float)V * 0.5f + 1.0f;
  int row_id = 0;
  for (int i = 0; i < N; ++i) {
    float d0 = wrapHalf((float)i - state.offset, N);
    int mmax = (int)std::ceil(half / (float)N) + 1;
    for (int m = -mmax; m <= mmax; ++m) {
      float d = d0 + (float)m * (float)N;
      if (std::fabs(d) > half)
        continue;
      float t = smooth(std::clamp(1.0f - std::fabs(d), 0.0f, 1.0f));
      rows.push_back(Row{row_id++, i, d, std::lerp(slot, sel_size, t), 0.0f});
    }
  }
  std::sort(rows.begin(), rows.end(),
            [](const Row &a, const Row &b) { return a.d < b.d; });

  if (!rows.empty()) {
    int f = 0;
    for (int j = 0; j < (int)rows.size(); ++j) {
      if (std::fabs(rows[j].d) < std::fabs(rows[f].d))
        f = j;
    }
    rows[f].main_pos = centerMain(rows[f].d);
    for (int j = f - 1; j >= 0; --j)
      rows[j].main_pos =
          rows[j + 1].main_pos - (rows[j].h + rows[j + 1].h) * 0.5f;
    for (int j = f + 1; j < (int)rows.size(); ++j)
      rows[j].main_pos =
          rows[j - 1].main_pos + (rows[j - 1].h + rows[j].h) * 0.5f;
  }

  bool accept_tile_input = !state.menu_open && !panel_open;

  for (const Row &row : rows) {
    const App &app = state.apps[row.idx];
    float t = smooth(std::clamp(1.0f - std::fabs(row.d), 0.0f, 1.0f));

    float h_i = row.h;
    float w_i = flerp(cross_un, cross_sel, t);
    float x0, y0;
    if (horizontal) {
      x0 = row.main_pos - h_i * 0.5f;
      y0 = top ? 0.0f : H - w_i;
    } else {
      x0 = left ? 0.0f : W - w_i;
      y0 = row.main_pos - h_i * 0.5f;
    }
    ImVec2 min(x0, y0);
    ImVec2 max(x0 + (horizontal ? h_i : w_i), y0 + (horizontal ? w_i : h_i));

    if (accept_tile_input) {
      ImGui::PushID(row.id);
      ImGui::SetCursorScreenPos(min);
      ImGui::InvisibleButton("##tile", ImVec2(max.x - min.x, max.y - min.y));
      if (ImGui::IsItemClicked()) {
        if (t > 0.9f)
          actions.launch(app);
        else
          state.selected = row.idx;
      }
      ImGui::PopID();
    }

    float bright = flerp(0.72f, 1.0f, t);
    if (app.tile_type == 1) {
      ImU32 c1 = colScaled(app.color1, bright);
      ImU32 c2 = colScaled(app.color2, bright);
      dl->AddRectFilledMultiColor(min, max, c1, c2, c2, c1);
    } else {
      dl->AddRectFilled(min, max, colScaled(app.color1, bright));
    }

    std::string label = upper(app.name);

    if (!g_font_tile) {
      ImGui::PushFont(ImGui::GetDefaultFont());
    } else {
      ImGui::PushFont(g_font_tile);
    }

    float fs_sel = H * cfg.font_tile_pct;
    float fs = flerp(fs_sel * 0.78f, fs_sel, t);
    ImVec2 ts = g_font_tile ? g_font_tile->CalcTextSizeA(fs, 10000.0f, 0.0f,
                                                         label.c_str(), nullptr)
                            : ImGui::CalcTextSize(label.c_str());

    float isz = flerp(icon_un, icon_sel, t);

    ImU32 label_col =
        app.has_icon_tint
            ? IM_COL32(app.icon_tint.r, app.icon_tint.g, app.icon_tint.b, 255)
            : IM_COL32(255, 255, 255, 255);

    if (horizontal) {
      float icon_x = x0 + (h_i - isz) * 0.5f;
      float label_x = x0 + (h_i - ts.x) * 0.5f;
      float icon_y, label_y;

      if (top) {
        // label pegado arriba; icono centrado en el resto
        label_y = y0 + w_i * 0.05f;
        float area_top = label_y + ts.y;
        icon_y = area_top + ((y0 + w_i) - area_top - isz) * 0.5f;
      } else {
        // label pegado abajo; icono centrado en el resto
        label_y = y0 + w_i * 0.95f - ts.y;
        icon_y = y0 + (label_y - y0 - isz) * 0.5f;
      }

      if (app.icon_texture) {
        dl->AddImage((ImTextureID)app.icon_texture, ImVec2(icon_x, icon_y),
                     ImVec2(icon_x + isz, icon_y + isz));
      }
      dl->AddText(g_font_tile, fs, ImVec2(label_x, label_y), label_col,
                  label.c_str());
    } else {
      if (app.icon_texture) {
        ImVec2 imin(x0 + pad, row.main_pos - isz * 0.5f);
        dl->AddImage((ImTextureID)app.icon_texture, imin,
                     ImVec2(imin.x + isz, imin.y + isz));
      }
      dl->AddText(g_font_tile, fs,
                  ImVec2(x0 + pad + isz + pad, row.main_pos - ts.y * 0.5f),
                  label_col, label.c_str());
    }
    ImGui::PopFont();
  }

  if (panel_open) {
    if (state.show_controllers)
      drawControllersPanel(state, const_cast<Config &>(cfg), actions);
    else
      drawPanel(state, const_cast<Config &>(cfg), actions, state.show_settings);
  } else { // <-- ESTE BLOQUE FALTABA
    if (state.menu_anim > 0.001f) {
      dl->AddRectFilled(ImVec2(0, 0), ImVec2(W, H),
                        IM_COL32(0, 0, 0, (int)(130 * state.menu_anim)));
    }
    drawSystemMenu(state, cfg, actions, W, H);
  }

  bool clock_bottom = horizontal && top;

  drawClock(cfg, vp, left, clock_bottom);
  if (cfg.show_player_indicators) {
    drawPlayerIndicators(state, cfg, actions.player_status(), vp);
  }
  if (!panel_open) {
    drawHints(state, cfg, vp);
  }

  ImGui::End();
  ImGui::PopStyleVar();
}
