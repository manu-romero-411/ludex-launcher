#include "shell_ui.h"
#include "ui/ui_common.h"
#include "ui/components.h"
#include "ui/system_menu.h"
#include "ui/panels.h"
#include "ui/tile_carousel.h"
#include "ui/wallpaper.h"
#include <imgui.h>

// Instancias globales de componentes
static ui::SystemClock g_clock;
static ui::EdgeFades g_edge_fades;
static ui::HelpHints g_help_hints;
static ui::PlayerIndicators g_player_indicators;
static ui::SystemMenu g_system_menu;
static ui::SettingsPanel g_settings_panel;
static ui::ControllersPanel g_controllers_panel;
static ui::TileCarousel g_tile_carousel;

// ============================================================================
// Input Handlers (lógica de navegación, no dibujo)
// ============================================================================
void controllersInput(ShellState& st, Config& cfg, const ShellActions& actions, UiAction a) {
    if (!st.show_controllers) return;
    auto devs = actions.devices ? actions.devices() : std::vector<InputManager::DeviceInfo>{};

    if (st.controller_pick_player < 0) {
        const int count = 9;
        int& f = st.controllers_focus;
        switch (a) {
        case UiAction::Up: f = (f - 1 + count) % count; break;
        case UiAction::Down: f = (f + 1) % count; break;
        case UiAction::Select:
            if (f < 8) { st.controller_pick_player = f; st.controller_pick_focus = 0; }
            else st.show_controllers = false;
            break;
        case UiAction::Back:
        case UiAction::Guide: st.show_controllers = false; break;
        default: break;
        }
    } else {
        const int rows = 1 + (int)devs.size();
        const int count = rows + 1;
        int& f = st.controller_pick_focus;
        switch (a) {
        case UiAction::Up: f = (f - 1 + count) % count; break;
        case UiAction::Down: f = (f + 1) % count; break;
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
                if (actions.apply_controllers) actions.apply_controllers();
                cfg.save(cfg.ini_path);
                st.controller_pick_player = -1;
            } else st.controller_pick_player = -1;
            break;
        case UiAction::Back:
        case UiAction::Guide: st.controller_pick_player = -1; break;
        default: break;
        }
    }
}

void panelInput(ShellState& st, Config& cfg, const ShellActions& actions, UiAction a) {
    bool settings = st.show_settings;
    if (!settings && !st.show_power) return;

    // Las filas están definidas en panels.cpp, necesitamos acceso a ellas
    // Por simplicidad, replicamos la lógica de navegación aquí
    int count = settings ? 7 : 4; // kSettingsCount : kPowerCount
    int* pf = settings ? &st.settings_focus : &st.power_focus;

    switch (a) {
    case UiAction::Up: *pf = (*pf - 1 + count) % count; break;
    case UiAction::Down: *pf = (*pf + 1) % count; break;
    case UiAction::Left:
    case UiAction::Right:
        // Para ajustes left/right, necesitamos llamar a adjust() de la fila
        // Esto requiere exponer las filas o hacer un callback
        // Por ahora, solo navegación vertical
        break;
    case UiAction::Select:
        // Similar: necesitamos activar la fila
        break;
    case UiAction::Back:
    case UiAction::Guide:
        if (settings) { cfg.save(cfg.ini_path); st.show_settings = false; }
        else st.show_power = false;
        break;
    default: break;
    }
}

// ============================================================================
// Main Orchestrator
// ============================================================================
void loadShellFonts(const Config& cfg, float screen_h) {
    ui::loadShellFonts(cfg, screen_h);
}

void drawShellImGui(ShellState& state, const Config& cfg, const ShellActions& actions) {
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    float W = vp->WorkSize.x, H = vp->WorkSize.y;
    bool panel_open = state.show_settings || state.show_power || state.show_controllers;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::SetNextWindowPos(vp->WorkPos);
    ImGui::SetNextWindowSize(vp->WorkSize);
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground |
                             ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoScrollbar |
                             ImGuiWindowFlags_NoScrollWithMouse;
    ImGui::Begin("##ludex", nullptr, flags);
    ImDrawList* dl = ImGui::GetWindowDrawList();

    // Wallpaper
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
            ui::drawWallpaperLayer(dl, W, H, state.wallpapers[state.wp_current], tint_cur);
            ui::drawWallpaperLayer(dl, W, H, state.wallpapers[state.wp_next], tint_nxt);
        } else {
            ui::drawWallpaperLayer(dl, W, H, state.wallpapers[state.wp_current], IM_COL32(255, 255, 255, 255));
        }
        dl->AddRectFilled(ImVec2(0, 0), ImVec2(W, H), overlay);
    } else {
        bool light = (cfg.theme == "light");
        dl->AddRectFilled(ImVec2(0, 0), ImVec2(W, H),
                          light ? IM_COL32(245, 245, 245, 255) : IM_COL32(18, 18, 20, 255));
    }

    // Componentes
    g_edge_fades.draw(cfg, W, H);
    g_tile_carousel.draw(state, cfg, actions, W, H, panel_open);

    if (panel_open) {
        if (state.show_controllers)
            g_controllers_panel.draw(state, const_cast<Config&>(cfg), actions);
        else
            g_settings_panel.draw(state, const_cast<Config&>(cfg), actions, state.show_settings);
    } else {
        if (state.menu_anim > 0.001f) {
            dl->AddRectFilled(ImVec2(0, 0), ImVec2(W, H),
                              IM_COL32(0, 0, 0, (int)(130 * state.menu_anim)));
        }
        g_system_menu.draw(state, cfg, actions, W, H);
    }

    bool horizontal = (cfg.side == "top" || cfg.side == "bottom");
    bool left = (cfg.side == "left");
    bool top = (cfg.side == "top");
    bool clock_bottom = horizontal && top;
    g_clock.draw(cfg, vp, left, clock_bottom);

    if (cfg.show_player_indicators) {
        g_player_indicators.draw(state, cfg, actions.player_status(), vp);
    }

    if (!panel_open) {
        g_help_hints.draw(state, cfg, vp);
    }

    ImGui::End();
    ImGui::PopStyleVar();
}