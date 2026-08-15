#include "ui_input_handlers.h"
#include "../input/input_manager.h"
#include "panels/pan_bluetooth.h"
#include "panels/pan_bluetooth_scan.h"
#include "panels/pan_settings.h"
#include "panels/pan_shutdown.h"
#include "panels/panel_renderer.h"
#include "shell/shell_state.h"
#include "ui/panels/pan_mainmenu.h"
#include "ui/panels/pan_pin.h"

void controllersInput(ShellState &st, Config &cfg, const ShellActions &actions,
                      UiAction a) {
  if (!st.show_controllers)
    return;
  auto devs = actions.devices ? actions.devices()
                              : std::vector<InputManager::DeviceInfo>{};
  if (st.controller_pick_player < 0) {
    const int count = 9; // 8 players + BACK
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
      } else {
        st.show_controllers = false;
        st.show_system = true;
        st.system_focus = 0;
      }
      break;
    case UiAction::Back:
    case UiAction::Guide:
      st.show_controllers = false;
      st.show_system = true;
      st.system_focus = 0;
      break;
    default:
      break;
    }
  } else {
    const int rows = 1 + (int)devs.size(); // DEFAULT + devices
    const int count = rows + 1;            // + CANCEL
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
      } else {
        st.controller_pick_player = -1;
      }
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

void panelInput(ShellState &st, Config &cfg, const ShellActions &actions,
                UiAction a) {
  // El modal PIN tiene prioridad absoluta
  if (st.show_pin) {
    if (a == UiAction::Back || a == UiAction::Guide) {
      if (actions.bluetooth_cancel_pin)
        actions.bluetooth_cancel_pin();
      st.show_pin = false;
      return;
    }
    auto spec = ui::panels::makePinPanelSpec(st, actions);
    ui::panels::handlePanelAction(spec, st, cfg, actions, a);
    return;
  }
  // Desvincular dispositivo con X (Alt) en el panel Bluetooth
  if (a == UiAction::Alt && st.show_bluetooth) {
    auto spec = ui::panels::makeBluetoothPanelSpec(st, cfg, actions);
    int f = st.bluetooth_focus;
    if (f >= 0 && f < (int)spec.rows.size() && !spec.rows[f].tag.empty()) {
      if (actions.bluetooth_remove)
        actions.bluetooth_remove(spec.rows[f].tag);
    }
    return;
  }
  if (st.show_system) {
    auto spec = ui::panels::makeSystemPanelSpec(st, actions);
    ui::panels::handlePanelAction(spec, st, cfg, actions, a);
  } else if (st.show_settings) {
    auto spec = ui::panels::makeSettingsPanelSpec(st, cfg, actions);
    ui::panels::handlePanelAction(spec, st, cfg, actions, a);
  } else if (st.show_power) {
    auto spec = ui::panels::makeShutdownPanelSpec(st, actions);
    ui::panels::handlePanelAction(spec, st, cfg, actions, a);
  } else if (st.show_bluetooth) {
    auto spec = ui::panels::makeBluetoothPanelSpec(st, cfg, actions);
    ui::panels::handlePanelAction(spec, st, cfg, actions, a);
  } else if (st.show_bluetooth_scan) {

    // Si pulsaron B (Back) o Guide, cancelamos el escaneo antes de cerrar el
    // panel.
    if ((a == UiAction::Back || a == UiAction::Guide) &&
        actions.bluetooth_cancel_scan) {
      actions.bluetooth_cancel_scan();
    }

    auto spec = ui::panels::makeBluetoothScanPanelSpec(st, cfg, actions);
    ui::panels::handlePanelAction(spec, st, cfg, actions, a);
  }
}