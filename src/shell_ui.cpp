#include "shell_ui.h"

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
#include "imgui_internal.h"
#include "input_manager.h"

/* ================================================================
 * Fuentes y utilidades
 * ================================================================ */

static ImFont* g_font_tile = nullptr;
static ImFont* g_font_clock = nullptr;
static ImFont* g_font_date = nullptr;
static ImFont* g_font_hint = nullptr;

void loadShellFonts(const Config& cfg, float screen_h) {
    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->Clear();
    std::error_code ec;

    // Tamaños mínimos seguros
    float size_tile = std::max(14.0f, screen_h * cfg.font_tile_pct);
    float size_clock = std::max(16.0f, screen_h * cfg.clock_pct);
    float size_date = std::max(12.0f, screen_h * cfg.date_pct);
    float size_hint = std::max(10.0f, screen_h * cfg.font_hint_pct);

    // Cargar fuente bold para tiles
    if (std::filesystem::exists(cfg.font_bold, ec)) {
        g_font_tile = io.Fonts->AddFontFromFileTTF(
            cfg.font_bold.c_str(), size_tile);
        if (!g_font_tile) {
            std::cerr << "[ludex] AddFontFromFileTTF falló para: "
                      << cfg.font_bold << "\n";
        }
    }
    if (!g_font_tile) {
        std::cerr << "[ludex] Usando fuente por defecto para tiles\n";
        g_font_tile = io.Fonts->AddFontDefault();
    }

    // Cargar fuente regular para reloj/fecha/hints
    ImFont* regular = nullptr;
    if (std::filesystem::exists(cfg.font_regular, ec)) {
        regular = io.Fonts->AddFontFromFileTTF(
            cfg.font_regular.c_str(), size_clock);
        if (!regular) {
            std::cerr << "[ludex] AddFontFromFileTTF falló para: "
                      << cfg.font_regular << "\n";
        }
    }
    if (!regular) {
        std::cerr << "[ludex] Usando fuente por defecto para texto regular\n";
        regular = io.Fonts->AddFontDefault();
    }

    g_font_clock = regular;

    // Para fecha y hints, reusamos la misma fuente pero con distinto tamaño
    // (ImGui permite múltiples tamaños de la misma fuente en el atlas)
    g_font_date = io.Fonts->AddFontFromFileTTF(
        cfg.font_regular.c_str(), size_date);
    if (!g_font_date) g_font_date = regular;

    g_font_hint = io.Fonts->AddFontFromFileTTF(
        cfg.font_regular.c_str(), size_hint);
    if (!g_font_hint) g_font_hint = regular;

    // FORZAR construcción del atlas antes de que el backend lo use
    unsigned char* pixels;
    int width, height;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

    if (width == 0 || height == 0) {
        std::cerr << "[ludex] ERROR: Font atlas vacío después de GetTexDataAsRGBA32\n";
    }

    std::cerr << "[ludex] Fuentes cargadas: tile=" << (g_font_tile ? "OK" : "NULL")
              << " clock=" << (g_font_clock ? "OK" : "NULL")
              << " atlas=" << width << "x" << height << "\n";
}

static std::string upper(std::string s) {
    for (char& c : s) c = (char)std::toupper((unsigned char)c);
    return s;
}

static ImU32 colRGBA(int r, int g, int b, int a = 255) {
    return IM_COL32(r, g, b, a);
}

static ImU32 colScaled(const TileColor& c, float f, int a = 255) {
    return IM_COL32(
        (int)std::clamp(c.r * f, 0.0f, 255.0f),
        (int)std::clamp(c.g * f, 0.0f, 255.0f),
        (int)std::clamp(c.b * f, 0.0f, 255.0f),
        a);
}

static float flerp(float a, float b, float t) { return a + (b - a) * t; }
static float smooth(float t) { return t * t * (3.0f - 2.0f * t); }

static std::string fmt3(float v) {
    char b[32]; std::snprintf(b, sizeof(b), "%.3f", v); return b;
}

/* ================================================================
 * Elementos fijos: reloj, fades de borde, menú pequeño de abajo, hints
 * ================================================================ */

static void drawClock(const Config& cfg, const ImGuiViewport* vp,
                      bool left_side, bool bottom) {
    std::time_t now = std::time(nullptr);
    std::tm* local = std::localtime(&now);

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

static void drawPlayerIndicators(
    const std::vector<InputManager::PlayerStatus>& players,
    const ImGuiViewport* vp, bool left_side, bool bottom
) {
    if (players.empty()) return;

    ImGui::PushFont(g_font_hint);
    float margin = vp->WorkSize.x * 0.018f;

    float y;
    ImDrawList* dl = ImGui::GetWindowDrawList();

    if (bottom) {
        // encima del bloque de reloj inferior
        ImGui::PushFont(g_font_clock);
        float ch = ImGui::CalcTextSize("00:00:00").y;
        ImGui::PopFont();
        ImGui::PushFont(g_font_date);
        float dh = ImGui::CalcTextSize("0").y;
        ImGui::PopFont();
        y = vp->WorkSize.y - margin - dh - 6.0f - ch - 34.0f;
    } else {
        y = margin + 80.0f;
    }

    if (left_side) {
        float x = vp->WorkSize.x - margin;
        for (const auto& p : players) {
            std::string label = "J" + std::to_string(p.player + 1);
            ImVec2 ts = ImGui::CalcTextSize(label.c_str());
            x -= (ts.x + 30.0f);

            ImU32 color = p.active
                ? IM_COL32(100, 255, 120, 255)   // verde brillante si activo
                : IM_COL32(80, 80, 85, 200);     // gris oscuro si inactivo

            dl->AddCircleFilled(
                ImVec2(x + ts.x + 14.0f, y + ts.y * 0.5f),
                7.0f, color);

            if (p.active) {
                dl->AddCircle(
                    ImVec2(x + ts.x + 14.0f, y + ts.y * 0.5f),
                    10.0f, IM_COL32(100, 255, 120, 180), 0, 2.0f);
            }

            ImGui::SetCursorScreenPos(ImVec2(x, y));
            ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.85f, 1.0f), "%s", label.c_str());
        }
    } else {
        float x = margin;
        for (const auto& p : players) {
            std::string label = "J" + std::to_string(p.player + 1);
            ImVec2 ts = ImGui::CalcTextSize(label.c_str());

            ImU32 color = p.active
                ? IM_COL32(100, 255, 120, 255)
                : IM_COL32(80, 80, 85, 200);

            dl->AddCircleFilled(
                ImVec2(x + 7.0f, y + ts.y * 0.5f),
                7.0f, color);

            if (p.active) {
                dl->AddCircle(
                    ImVec2(x + 7.0f, y + ts.y * 0.5f),
                    10.0f, IM_COL32(100, 255, 120, 180), 0, 2.0f);
            }

            ImGui::SetCursorScreenPos(ImVec2(x + 20.0f, y));
            ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.85f, 1.0f), "%s", label.c_str());
            x += ts.x + 36.0f;
        }
    }
    ImGui::PopFont();
}

static void drawEdgeFades(const Config& cfg, float W, float H) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    int a = (int)std::clamp(cfg.edge_fade_alpha, 0.0f, 255.0f);
    bool light = (cfg.theme == "light");

    ImU32 edge = light ? IM_COL32(245, 245, 245, a) : IM_COL32(0, 0, 0, a);
    ImU32 none = light ? IM_COL32(245, 245, 245, 0) : IM_COL32(0, 0, 0, 0);

    float left_fw = W * (cfg.tile_sel_w_pct + cfg.edge_fade_pct);
    dl->AddRectFilledMultiColor(
        ImVec2(0.0f, 0.0f), ImVec2(left_fw, H),
        edge, none, none, edge);

    float right_fw = W * cfg.edge_fade_pct;
    dl->AddRectFilledMultiColor(
        ImVec2(W - right_fw, 0.0f), ImVec2(W, H),
        none, edge, edge, none);
}

static void drawSystemMenu(
    const ShellState& state, const Config& cfg,
    const ShellActions& actions, float W, float H
) {
    if (state.menu_anim <= 0.001f) return;

    ImDrawList* dl = ImGui::GetWindowDrawList();
    bool left = (cfg.side == "left" || cfg.side == "bottom");
    bool light = (cfg.theme == "light");

    ImU32 row_normal = light ? IM_COL32(230, 232, 240, 255) : IM_COL32(32, 32, 36, 255);
    ImU32 row_focus  = light ? IM_COL32(200, 210, 230, 255) : IM_COL32(74, 74, 80, 255);
    ImU32 text_col   = light ? IM_COL32(25, 25, 30, 255) : IM_COL32(230, 230, 230, 255);

    float menu_h = H * cfg.menu_h_pct;
    float menu_w = W * cfg.tile_sel_w_pct;
    float total = menu_h * 3.0f;
    float y_base = H - total * state.menu_anim;

    static const char* labels[3] = {
        "CONFIGURACI\xC3\x93N", "SALIR", "APAGAR PC..."
    };

    for (int m = 0; m < 3; ++m) {
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
                if (m == 0) actions.open_settings();
                else if (m == 1) actions.quit();
                else actions.poweroff();
            }
            ImGui::PopID();
        }

        std::string label = labels[m];
        ImGui::PushFont(g_font_tile);
        ImVec2 ts = ImGui::CalcTextSize(label.c_str());
        dl->AddText(
            ImVec2(min.x + W * 0.012f, y0 + (menu_h - ts.y) * 0.5f),
            text_col, label.c_str());
        ImGui::PopFont();
    }
}

static void drawHints(const ShellState& state, const ImGuiViewport* vp) {
    float W = vp->WorkSize.x, H = vp->WorkSize.y;
    ImDrawList* dl = ImGui::GetWindowDrawList();

    const char* text;
    if (state.show_settings)
        text = "UP/DOWN Navigate    LEFT/RIGHT Change    A Accept    B Back";
    else if (state.show_power)
        text = "UP/DOWN Navigate    A Accept    B Back";
    else if (state.menu_open)
        text = "UP/DOWN Navigate    A Accept    B / HOME Close";
    else
        text = "UP/DOWN Navigate    A Select    B Back    HOME Menu";

    ImGui::PushFont(g_font_hint);
    ImVec2 ts = ImGui::CalcTextSize(text);
    float margin = vp->WorkSize.y * 0.018f;
    ImVec2 pos((W - ts.x) * 0.5f, H - margin - ts.y);   // <-- centrado
    dl->AddText(ImVec2(pos.x + 2, pos.y + 2), IM_COL32(0, 0, 0, 160), text);
    dl->AddText(pos, IM_COL32(235, 235, 235, 200), text);
    ImGui::PopFont();
}

/* ================================================================
 * Paneles estilo EmulationStation (settings + power)
 * ================================================================ */

namespace {

struct SRow {
    const char* label;
    bool adjustable;
    std::string (*get)(const Config&);
    void (*adjust)(Config&, int dir);
    void (*activate)(ShellState&, Config&, const ShellActions&);
};

const SRow kSettingsRows[] = {
    {"LADO SIDEBAR", true,
     [](const Config& c) { return c.side; },
     [](Config& c, int) { 
         if (c.side == "left") c.side = "right";
         else if (c.side == "right") c.side = "top";
         else if (c.side == "top") c.side = "bottom";
         else c.side = "left";
     },
     nullptr},
    {"ELEMENTOS VISIBLES", true,
     [](const Config& c) { return std::to_string(c.visible_items); },
     [](Config& c, int d) {
         c.visible_items = std::clamp(c.visible_items + 2 * d, 5, 11);
         if (c.visible_items % 2 == 0) c.visible_items--;
     }, nullptr},
    {"ANCHO TILE", true,
     [](const Config& c) { return fmt3(c.tile_w_pct); },
     [](Config& c, int d) {
         c.tile_w_pct = std::clamp(c.tile_w_pct + 0.005f * d, 0.10f, 0.40f);
     }, nullptr},
    {"ANCHO TILE SELECCION", true,
     [](const Config& c) { return fmt3(c.tile_sel_w_pct); },
     [](Config& c, int d) {
         c.tile_sel_w_pct = std::clamp(c.tile_sel_w_pct + 0.005f * d, 0.10f, 0.45f);
     }, nullptr},
    {"ALTURA RELOJ (REINICIO)", true,
     [](const Config& c) { return fmt3(c.clock_pct); },
     [](Config& c, int d) {
         c.clock_pct = std::clamp(c.clock_pct + 0.002f * d, 0.02f, 0.12f);
     }, nullptr},
    {"ALTURA FECHA (REINICIO)", true,
     [](const Config& c) { return fmt3(c.date_pct); },
     [](Config& c, int d) {
         c.date_pct = std::clamp(c.date_pct + 0.001f * d, 0.01f, 0.06f);
     }, nullptr},
    {"SOMBRA BORDES", true,
     [](const Config& c) { return std::to_string((int)c.edge_fade_alpha); },
     [](Config& c, int d) {
         c.edge_fade_alpha = std::clamp(c.edge_fade_alpha + 5.0f * d, 0.0f, 255.0f);
     }, nullptr},
    {"MOSTRAR MANDOS", true,
     [](const Config& c) -> std::string { return c.show_player_indicators ? "SI" : "NO"; },
     [](Config& c, int) { c.show_player_indicators = !c.show_player_indicators; },
     nullptr},
    {"TEMA", true,
     [](const Config& c) { return c.theme; },
     [](Config& c, int) { c.theme = (c.theme == "dark") ? "light" : "dark"; },
     nullptr},
    {"GUARDAR CAMBIOS", false, nullptr, nullptr,
     [](ShellState&, Config& c, const ShellActions&) { c.save(c.ini_path); }},
    {"VOLVER", false, nullptr, nullptr,
     [](ShellState& s, Config&, const ShellActions&) { s.show_settings = false; }},
};

const SRow kPowerRows[] = {
    {"SUSPENDER", false, nullptr, nullptr,
     [](ShellState& s, Config&, const ShellActions& a) {
         if (a.suspend) a.suspend(); s.show_power = false;
     }},
    {"REINICIAR", false, nullptr, nullptr,
     [](ShellState& s, Config&, const ShellActions& a) {
         if (a.reboot) a.reboot(); s.show_power = false;
     }},
    {"APAGAR", false, nullptr, nullptr,
     [](ShellState& s, Config&, const ShellActions& a) {
         if (a.poweroff) a.poweroff(); s.show_power = false;
     }},
    {"VOLVER", false, nullptr, nullptr,
     [](ShellState& s, Config&, const ShellActions&) { s.show_power = false; }},
};

constexpr int kSettingsCount = (int)(sizeof(kSettingsRows) / sizeof(kSettingsRows[0]));
constexpr int kPowerCount    = (int)(sizeof(kPowerRows) / sizeof(kPowerRows[0]));

} // namespace

void panelInput(ShellState& st, Config& cfg,
                const ShellActions& actions, UiAction a) {
    bool settings = st.show_settings;
    if (!settings && !st.show_power) return;

    const SRow* rows = settings ? kSettingsRows : kPowerRows;
    int count = settings ? kSettingsCount : kPowerCount;
    int* pf = settings ? &st.settings_focus : &st.power_focus;

    switch (a) {
        case UiAction::Up:    *pf = (*pf - 1 + count) % count; break;
        case UiAction::Down:  *pf = (*pf + 1) % count; break;
        case UiAction::Left:  if (rows[*pf].adjust) rows[*pf].adjust(cfg, -1); break;
        case UiAction::Right: if (rows[*pf].adjust) rows[*pf].adjust(cfg, +1); break;
        case UiAction::Select:
            if (rows[*pf].activate) rows[*pf].activate(st, cfg, actions);
            else if (rows[*pf].adjust) rows[*pf].adjust(cfg, +1);
            break;
        case UiAction::Back:
        case UiAction::Guide:
            if (settings) st.show_settings = false;
            else st.show_power = false;
            break;
        default: break;
    }
}

static void drawPanel(ShellState& st, Config& cfg,
                      const ShellActions& actions, bool settings) {
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    float W = vp->WorkSize.x, H = vp->WorkSize.y;
    ImDrawList* dl = ImGui::GetWindowDrawList();

    bool light = (cfg.theme == "light");

    // Colores según tema
    ImU32 fade_col    = light ? IM_COL32(240, 240, 240, 210) : IM_COL32(0, 0, 0, 210);
    ImU32 panel_bg    = light ? IM_COL32(250, 250, 252, 250) : IM_COL32(30, 32, 40, 250);
    ImU32 panel_title = light ? IM_COL32(230, 232, 240, 255) : IM_COL32(44, 47, 60, 255);
    ImU32 row_focus   = light ? IM_COL32(210, 220, 240, 255) : IM_COL32(78, 82, 98, 255);
    ImU32 text_main   = light ? IM_COL32(25, 25, 30, 255) : IM_COL32(230, 230, 230, 255);
    ImU32 text_val    = light ? IM_COL32(40, 40, 50, 255) : IM_COL32(240, 240, 240, 255);

    dl->AddRectFilled(ImVec2(0, 0), ImVec2(W, H), fade_col);

    const SRow* rows = settings ? kSettingsRows : kPowerRows;
    int count = settings ? kSettingsCount : kPowerCount;
    int focus = settings ? st.settings_focus : st.power_focus;
    const char* title = settings ? "AJUSTES DE INTERFAZ" : "APAGADO DEL EQUIPO";

    float row_h = H * 0.055f;
    float title_h = H * 0.09f;
    float pw = W * 0.56f;
    float ph = title_h + count * row_h + row_h * 0.8f;
    float px = (W - pw) * 0.5f;
    float py = (H - ph) * 0.5f;
    float pad = W * 0.012f;

    dl->AddRectFilled(ImVec2(px, py), ImVec2(px + pw, py + ph),
                      panel_bg, 10.0f);
    dl->AddRectFilled(ImVec2(px, py), ImVec2(px + pw, py + title_h),
                      panel_title, 10.0f, ImDrawFlags_RoundCornersTop);

    ImGui::PushFont(g_font_tile);
    ImVec2 tt = ImGui::CalcTextSize(title);
    dl->AddText(ImVec2(px + (pw - tt.x) * 0.5f, py + (title_h - tt.y) * 0.5f),
                text_main, title);

    for (int i = 0; i < count; ++i) {
        float y0 = py + title_h + i * row_h;
        ImVec2 rmin(px, y0), rmax(px + pw, y0 + row_h);

        if (i == focus) dl->AddRectFilled(rmin, rmax, row_focus);

        ImGui::PushID(9000 + i);
        ImGui::SetCursorScreenPos(rmin);
        ImGui::InvisibleButton("##row", ImVec2(pw, row_h));
        if (ImGui::IsItemClicked()) {
            (settings ? st.settings_focus : st.power_focus) = i;
            if (rows[i].activate) rows[i].activate(st, cfg, actions);
            else if (rows[i].adjust) rows[i].adjust(cfg, +1);
        }
        ImGui::PopID();

        dl->AddText(ImVec2(px + pad, y0 + (row_h - tt.y) * 0.5f),
                    text_main, rows[i].label);

        if (rows[i].adjustable && rows[i].get) {
            std::string val = "<  " + rows[i].get(cfg) + "  >";
            ImVec2 vs = ImGui::CalcTextSize(val.c_str());
            dl->AddText(ImVec2(px + pw - pad - vs.x, y0 + (row_h - vs.y) * 0.5f),
                        text_val, val.c_str());
        }
    }
    ImGui::PopFont();
}

/* ================================================================
 * Orquestación principal
 * ================================================================ */

void drawShellImGui(
    ShellState& state,
    const Config& cfg,
    const ShellActions& actions
) {
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    float W = vp->WorkSize.x, H = vp->WorkSize.y;
    bool panel_open = state.show_settings || state.show_power;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::SetNextWindowPos(vp->WorkPos);
    ImGui::SetNextWindowSize(vp->WorkSize);

    ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_NoBackground |
        ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoScrollWithMouse;

    ImGui::Begin("##ludex", nullptr, flags);
    ImDrawList* dl = ImGui::GetWindowDrawList();

    /* ---- fondo (wallpaper + overlay + fades) ---- */
    if (state.wallpaper_texture && state.wallpaper_w > 0 && state.wallpaper_h > 0) {
        float iw = (float)state.wallpaper_w, ih = (float)state.wallpaper_h;
        float scale = std::max(W / iw, H / ih);
        float vis_w = W / scale, vis_h = H / scale;
        ImVec2 uv0((iw - vis_w) * 0.5f / iw, (ih - vis_h) * 0.5f / ih);
        ImVec2 uv1(uv0.x + vis_w / iw, uv0.y + vis_h / ih);
        dl->AddImage((ImTextureID)state.wallpaper_texture,
                     ImVec2(0, 0), ImVec2(W, H), uv0, uv1);
        dl->AddRectFilled(ImVec2(0, 0), ImVec2(W, H), IM_COL32(0, 0, 0, 90));
    } else {
        dl->AddRectFilled(ImVec2(0, 0), ImVec2(W, H), IM_COL32(18, 18, 20, 255));
    }

    drawEdgeFades(cfg, W, H);

    /* ---- sidebar de apps ---- */
    bool horizontal = (cfg.side == "top" || cfg.side == "bottom");
    bool left = (cfg.side == "left");
    bool top = (cfg.side == "top");
    
    int V = cfg.visible_items;
    float k = cfg.tile_sel_ratio;
    int N = (int)state.apps.size();

    // Dimensiones según orientación
    float main_axis = horizontal ? W : H;           // eje del carrusel
    float cross_axis = horizontal ? H : W;          // eje perpendicular
    
    float slot = main_axis / ((float)(V - 1) + k);
    float sel_size = slot * k;
    float cross_un = cross_axis * cfg.tile_w_pct;
    float cross_sel = cross_axis * cfg.tile_sel_w_pct;
    float icon_un = cross_axis * cfg.icon_pct * (horizontal ? 1.5f : 1.0f);
    float icon_sel = cross_axis * cfg.icon_sel_pct * (horizontal ? 1.5f : 1.0f);
    float pad = cross_axis * 0.012f;

    // Centro del eje principal (con animación suave)
    auto centerMain = [&](float d) -> float {
        float ad = std::fabs(d);
        float sgn = d >= 0.0f ? 1.0f : -1.0f;
        float s = sgn * smooth(std::min(ad, 1.0f));
        return main_axis * 0.5f + (sel_size - slot) * 0.5f * s + slot * d;
    };

    struct Row { int id, idx; float d, h, main_pos; };
    std::vector<Row> rows;

    const float half = (float)V * 0.5f + 1.0f;
    int row_id = 0;
    for (int i = 0; i < N; ++i) {
        float d0 = wrapHalf((float)i - state.offset, N);
        int mmax = (int)std::ceil(half / (float)N) + 1;
        for (int m = -mmax; m <= mmax; ++m) {
            float d = d0 + (float)m * (float)N;
            if (std::fabs(d) > half) continue;
            float t = smooth(std::clamp(1.0f - std::fabs(d), 0.0f, 1.0f));
            rows.push_back(Row{row_id++, i, d, std::lerp(slot, sel_size, t), 0.0f});
        }
    }
    std::sort(rows.begin(), rows.end(),
              [](const Row& a, const Row& b) { return a.d < b.d; });

    if (!rows.empty()) {
        int f = 0;
        for (int j = 0; j < (int)rows.size(); ++j) {
            if (std::fabs(rows[j].d) < std::fabs(rows[f].d)) f = j;
        }
        rows[f].main_pos = centerMain(rows[f].d);
        for (int j = f - 1; j >= 0; --j)
            rows[j].main_pos = rows[j + 1].main_pos - (rows[j].h + rows[j + 1].h) * 0.5f;
        for (int j = f + 1; j < (int)rows.size(); ++j)
            rows[j].main_pos = rows[j - 1].main_pos + (rows[j - 1].h + rows[j].h) * 0.5f;
    }

    bool accept_tile_input = !state.menu_open && !panel_open;

    for (const Row& row : rows) {
        const App& app = state.apps[row.idx];
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
                if (t > 0.9f) actions.launch(app);
                else state.selected = row.idx;
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

                // ---- icono + label: UNA sola vez, según orientación ----
        std::string label = upper(app.name);

        // Fuente del label: tamaño completo en la seleccionada,
        // ~22% menor en inactivas (el atlas está al tamaño grande,
        // así que reducir es limpio).
        if (!g_font_tile) {
            // Fallback extremo: dibujar sin fuente (ImGui usará la default)
            ImGui::PushFont(ImGui::GetDefaultFont());
        } else {
            ImGui::PushFont(g_font_tile);
        }

        float fs_sel = H * cfg.font_tile_pct;
        float fs = flerp(fs_sel * 0.78f, fs_sel, t);
        ImVec2 ts = g_font_tile
            ? g_font_tile->CalcTextSizeA(fs, 10000.0f, 0.0f, label.c_str(), nullptr)
            : ImGui::CalcTextSize(label.c_str());


        float isz = flerp(icon_un, icon_sel, t);

        ImU32 label_col = app.has_icon_tint
            ? IM_COL32(app.icon_tint.r, app.icon_tint.g, app.icon_tint.b, 255)
            : IM_COL32(255, 255, 255, 255);

        if (horizontal) {
            float gap = cross_axis * 0.02f;
            float icon_x = x0 + (h_i - isz) * 0.5f;
            float label_x = x0 + (h_i - ts.x) * 0.5f;
            float icon_y, label_y;
            if (top) { label_y = y0 + w_i * 0.10f; icon_y = label_y + ts.y + gap; }
            else     { icon_y = y0 + w_i * 0.10f; label_y = icon_y + isz + gap; }

            if (app.icon_texture) {
                dl->AddImage((ImTextureID)app.icon_texture,
                             ImVec2(icon_x, icon_y),
                             ImVec2(icon_x + isz, icon_y + isz));
            }
            dl->AddText(g_font_tile, fs, ImVec2(label_x, label_y),
                        label_col, label.c_str());
        } else {
            if (app.icon_texture) {
                ImVec2 imin(x0 + pad, row.main_pos - isz * 0.5f);
                dl->AddImage((ImTextureID)app.icon_texture,
                             imin, ImVec2(imin.x + isz, imin.y + isz));
            }
            dl->AddText(g_font_tile, fs,
                        ImVec2(x0 + pad + isz + pad, row.main_pos - ts.y * 0.5f),
                        label_col, label.c_str());
        }
        ImGui::PopFont();
    }

    /* ---- capa superior: menú sistema O panel completo ---- */
    if (panel_open) {
        drawPanel(state, const_cast<Config&>(cfg), actions, state.show_settings);
    } else {
        if (state.menu_anim > 0.001f) {
            dl->AddRectFilled(ImVec2(0, 0), ImVec2(W, H),
                              IM_COL32(0, 0, 0, (int)(130 * state.menu_anim)));
        }
        drawSystemMenu(state, cfg, actions, W, H);
    }

    bool clock_bottom = horizontal && top;

    drawClock(cfg, vp, left, clock_bottom);
    if (cfg.show_player_indicators) {
        drawPlayerIndicators(actions.player_status(), vp, left, clock_bottom);
    }
    if (!panel_open) {
        drawHints(state, vp);
    }
    ImGui::End();
    ImGui::PopStyleVar();
}