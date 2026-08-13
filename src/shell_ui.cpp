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

    if (std::filesystem::exists(cfg.font_bold, ec)) {
        g_font_tile = io.Fonts->AddFontFromFileTTF(
            cfg.font_bold.c_str(),
            std::max(10.0f, screen_h * cfg.font_tile_pct));
    } else {
        std::cerr << "[ludex] FUENTE NO ENCONTRADA: " << cfg.font_bold
                  << " — usando fuente por defecto.\n";
        g_font_tile = io.Fonts->AddFontDefault();
    }

    if (std::filesystem::exists(cfg.font_regular, ec)) {
        g_font_clock = io.Fonts->AddFontFromFileTTF(
            cfg.font_regular.c_str(),
            std::max(12.0f, screen_h * cfg.clock_pct));
        g_font_date = io.Fonts->AddFontFromFileTTF(
            cfg.font_regular.c_str(),
            std::max(9.0f, screen_h * cfg.date_pct));
        g_font_hint = io.Fonts->AddFontFromFileTTF(
            cfg.font_regular.c_str(),
            std::max(9.0f, screen_h * cfg.font_hint_pct));
    } else {
        std::cerr << "[ludex] FUENTE NO ENCONTRADA: " << cfg.font_regular
                  << " — usando fuente por defecto.\n";
        g_font_clock = g_font_date = g_font_hint = io.Fonts->AddFontDefault();
    }
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

static void drawClock(const Config& cfg, const ImGuiViewport* vp, bool left_side) {
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

    ImGui::PushFont(g_font_clock);
    ImGui::SetCursorScreenPos(ImVec2(x_time, margin));
    ImGui::TextUnformatted(time_text);
    ImGui::PopFont();

    ImGui::PushFont(g_font_date);
    ImGui::SetCursorScreenPos(ImVec2(x_date, margin + ts.y + 6.0f));
    ImGui::TextColored(ImVec4(0.85f, 0.85f, 0.88f, 0.9f), "%s", date_text);
    ImGui::PopFont();
}

static void drawEdgeFades(const Config& cfg, float W, float H) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    int a = (int)std::clamp(cfg.edge_fade_alpha, 0.0f, 255.0f);
    ImU32 edge = IM_COL32(0, 0, 0, a);
    ImU32 none = IM_COL32(0, 0, 0, 0);

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
    bool left = (cfg.side != "right");

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

        ImU32 col = (state.menu_selected == m)
            ? IM_COL32(74, 74, 80, 255)
            : IM_COL32(32, 32, 36, 255);
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
            IM_COL32(230, 230, 230, 255), label.c_str());
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
    float margin = W * 0.018f;
    ImVec2 pos(W - margin - ts.x, H - margin - ts.y);
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
     [](Config& c, int) { c.side = (c.side == "left") ? "right" : "left"; },
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
    dl->AddRectFilled(ImVec2(0, 0), ImVec2(W, H),
                      light ? IM_COL32(240, 240, 240, 210)
                            : IM_COL32(0, 0, 0, 210));

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
                      IM_COL32(30, 32, 40, 250), 10.0f);
    dl->AddRectFilled(ImVec2(px, py), ImVec2(px + pw, py + title_h),
                      IM_COL32(44, 47, 60, 255), 10.0f, ImDrawFlags_RoundCornersTop);

    ImGui::PushFont(g_font_tile);
    ImVec2 tt = ImGui::CalcTextSize(title);
    dl->AddText(ImVec2(px + (pw - tt.x) * 0.5f, py + (title_h - tt.y) * 0.5f),
                IM_COL32(240, 240, 240, 255), title);

    for (int i = 0; i < count; ++i) {
        float y0 = py + title_h + i * row_h;
        ImVec2 rmin(px, y0), rmax(px + pw, y0 + row_h);

        if (i == focus) {
            dl->AddRectFilled(rmin, rmax, IM_COL32(78, 82, 98, 255));
        }

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
                    IM_COL32(230, 230, 230, 255), rows[i].label);

        if (rows[i].adjustable && rows[i].get) {
            std::string val = "<  " + rows[i].get(cfg) + "  >";
            ImVec2 vs = ImGui::CalcTextSize(val.c_str());
            dl->AddText(ImVec2(px + pw - pad - vs.x, y0 + (row_h - vs.y) * 0.5f),
                        IM_COL32(240, 240, 240, 255), val.c_str());
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
    bool left = (cfg.side != "right");
    int V = cfg.visible_items;
    float k = cfg.tile_sel_ratio;
    float slot_h = H / ((float)(V - 1) + k);
    float sel_h = slot_h * k;
    float w_un = W * cfg.tile_w_pct;
    float w_sel = W * cfg.tile_sel_w_pct;
    float icon_un = H * cfg.icon_pct;
    float icon_sel = H * cfg.icon_sel_pct;
    float pad = W * 0.012f;
    int N = (int)state.apps.size();

    auto centerY = [&](float d) -> float {
        float ad = std::fabs(d);
        float sgn = d >= 0.0f ? 1.0f : -1.0f;
        float s = sgn * smooth(std::min(ad, 1.0f));
        return H * 0.5f + (sel_h - slot_h) * 0.5f * s + slot_h * d;
    };

    struct Row { int id, idx; float d, h, y; };
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
            rows.push_back(Row{row_id++, i, d, std::lerp(slot_h, sel_h, t), 0.0f});
        }
    }
    std::sort(rows.begin(), rows.end(),
              [](const Row& a, const Row& b) { return a.d < b.d; });

    if (!rows.empty()) {
        int f = 0;
        for (int j = 0; j < (int)rows.size(); ++j) {
            if (std::fabs(rows[j].d) < std::fabs(rows[f].d)) f = j;
        }
        rows[f].y = centerY(rows[f].d);
        for (int j = f - 1; j >= 0; --j)
            rows[j].y = rows[j + 1].y - (rows[j].h + rows[j + 1].h) * 0.5f;
        for (int j = f + 1; j < (int)rows.size(); ++j)
            rows[j].y = rows[j - 1].y + (rows[j - 1].h + rows[j].h) * 0.5f;
    }

    // No aceptar clicks sobre tiles si hay un panel o menú abierto
    bool accept_tile_input = !state.menu_open && !panel_open;

    for (const Row& row : rows) {
        const App& app = state.apps[row.idx];
        float t = smooth(std::clamp(1.0f - std::fabs(row.d), 0.0f, 1.0f));

        float h_i = row.h;
        float w_i = lerp(w_un, w_sel, t);
        float x0 = left ? 0.0f : W - w_i;
        float y0 = row.y - h_i * 0.5f;
        ImVec2 min(x0, y0), max(x0 + w_i, y0 + h_i);

        if (accept_tile_input) {
            ImGui::PushID(row.id);
            ImGui::SetCursorScreenPos(min);
            ImGui::InvisibleButton("##tile", ImVec2(w_i, h_i));
            if (ImGui::IsItemClicked()) {
                if (t > 0.9f) actions.launch(app);
                else state.selected = row.idx;
            }
            ImGui::PopID();
        }

        float bright = lerp(0.72f, 1.0f, t);
        if (app.tile_type == 1) {
            ImU32 c1 = colScaled(app.color1, bright);
            ImU32 c2 = colScaled(app.color2, bright);
            dl->AddRectFilledMultiColor(min, max, c1, c2, c2, c1);
        } else {
            dl->AddRectFilled(min, max, colScaled(app.color1, bright));
        }

        if (app.icon_texture) {
            float isz = lerp(icon_un, icon_sel, t);
            ImVec2 imin(x0 + pad, row.y - isz * 0.5f);
            dl->AddImage((ImTextureID)app.icon_texture,
                         imin, ImVec2(imin.x + isz, imin.y + isz));
        }

        std::string label = upper(app.name);
        ImGui::PushFont(g_font_tile);
        ImVec2 ts = ImGui::CalcTextSize(label.c_str());
        dl->AddText(
            ImVec2(x0 + pad + lerp(icon_un, icon_sel, t) + pad, row.y - ts.y * 0.5f),
            colRGBA(255, 255, 255), label.c_str());
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

    /* ---- siempre visibles: reloj + hints ---- */
    drawClock(cfg, vp, left);
    drawHints(state, vp);

    ImGui::End();
    ImGui::PopStyleVar();
}