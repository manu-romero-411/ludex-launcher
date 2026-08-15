#include "input_manager.h"
#include "shell_state.h"

void controllersInput(ShellState& st, Config& cfg, const ShellActions& actions, UiAction a) {
    if (!st.show_controllers) return;
    auto devs = actions.devices ? actions.devices() : std::vector<InputManager::DeviceInfo>{};

    if (st.controller_pick_player < 0) {
        const int count = 9; // 8 players + BACK
        int& f = st.controllers_focus;
        switch (a) {
        case UiAction::Up:   f = (f - 1 + count) % count; break;
        case UiAction::Down: f = (f + 1) % count; break;
        case UiAction::Select:
            if (f < 8) {
                st.controller_pick_player = f;
                st.controller_pick_focus = 0;
            } else {
                st.show_controllers = false;
            }
            break;
        case UiAction::Back:
        case UiAction::Guide:
            st.show_controllers = false;
            break;
        default: break;
        }
    } else {
        const int rows = 1 + (int)devs.size(); // DEFAULT + devices
        const int count = rows + 1; // + CANCEL
        int& f = st.controller_pick_focus;
        switch (a) {
        case UiAction::Up:   f = (f - 1 + count) % count; break;
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
            } else {
                st.controller_pick_player = -1;
            }
            break;
        case UiAction::Back:
        case UiAction::Guide:
            st.controller_pick_player = -1;
            break;
        default: break;
        }
    }
}

void panelInput(ShellState& st, Config& cfg, const ShellActions& actions, UiAction a) {
    bool settings = st.show_settings;
    if (!settings && !st.show_power) return;

    // Este handler ahora es manejado por drawGenericPanel internamente
    // Solo necesitamos manejar navegación si el panel no está consumiendo el input
    // Por simplicidad, dejamos que el panel_renderer maneje todo el input
}