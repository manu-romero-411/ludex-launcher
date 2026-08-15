#include "pan_controllers.h"
#include "../../core/i18n.h"
#include "../../input/input_manager.h"

namespace ui::panels {

namespace {

std::string assignmentLabel(const Config &cfg,
                            const std::vector<InputManager::DeviceInfo> &devs,
                            int p) {
  if (cfg.controller_guid[p].empty())
    return "DEFAULT";
  for (const auto &d : devs)
    if (d.guid == cfg.controller_guid[p])
      return "#" + std::to_string(d.sdl_index) + " " + d.name;
  return "OFFLINE (" + cfg.controller_name[p] + ")";
}

} // anonymous namespace

PanelSpec makeControllersPanelSpec(ShellState &st, Config &cfg,
                                   const ShellActions &actions) {
  auto devs = actions.devices ? actions.devices()
                              : std::vector<InputManager::DeviceInfo>{};
  bool picking = st.controller_pick_player >= 0;

  PanelSpec spec;

  if (!picking) {
    spec.title = _("CONTROLLER ASSIGNMENT");
    spec.focus_ptr = &st.controllers_focus;
    spec.scroll_ptr = &st.settings_scroll;
    spec.on_back = [](ShellState &s, Config &) {
      s.show_controllers = false;
      s.show_system = true;
      s.system_focus = 0;
    };
    for (int p = 0; p < 8; ++p) {
      RowDefinition row;
      row.label = _("PLAYER ") + std::to_string(p + 1) + _(" PAD");
      row.kind = RowKind::Activator;
      row.icon = RowIcon::Gamepad;
      row.get_value = [&cfg, devs, p](const Config &) {
        return assignmentLabel(cfg, devs, p);
      };
      row.on_select = [&st, p](ShellState &s, Config &, const ShellActions &) {
        s.controller_pick_player = p;
        s.controller_pick_focus = 0;
      };
      spec.rows.push_back(row);
    }

    // Footer: BACK
    RowDefinition back_row;
    back_row.label = _("BACK");
    back_row.kind = RowKind::Footer;
    back_row.icon = RowIcon::Exit;
    back_row.on_select = [](ShellState &s, Config &, const ShellActions &) {
      s.show_controllers = false;
      s.show_system = true;
      s.system_focus = 0;
    };
    spec.rows.push_back(back_row);

  } else {
    // Modo picking: seleccionar dispositivo para un player
    spec.title =
        "PAD FOR PLAYER " + std::to_string(st.controller_pick_player + 1);
    spec.focus_ptr = &st.controller_pick_focus;
    spec.scroll_ptr = &st.settings_scroll;
    spec.on_back = [](ShellState &s, Config &) {
      s.controller_pick_player = -1;
    };

    // Fila DEFAULT
    RowDefinition default_row;
    default_row.label = _("DEFAULT");
    default_row.kind = RowKind::Activator;
    default_row.icon = RowIcon::None;
    default_row.on_select = [&st, &cfg, &actions](ShellState &s, Config &c,
                                                  const ShellActions &a) {
      int p = s.controller_pick_player;
      c.controller_guid[p].clear();
      c.controller_name[p].clear();
      if (a.apply_controllers)
        a.apply_controllers();
      c.save(c.ini_path);
      s.controller_pick_player = -1;
    };
    spec.rows.push_back(default_row);

    // Filas de dispositivos
    for (const auto &d : devs) {
      RowDefinition dev_row;
      dev_row.label = "#" + std::to_string(d.sdl_index) + " " + d.name;
      dev_row.kind = RowKind::Activator;
      dev_row.icon = RowIcon::Gamepad;
      dev_row.on_select = [&st, &cfg, &actions, guid = d.guid, name = d.name](
                              ShellState &s, Config &c, const ShellActions &a) {
        int p = s.controller_pick_player;
        c.controller_guid[p] = guid;
        c.controller_name[p] = name;
        if (a.apply_controllers)
          a.apply_controllers();
        c.save(c.ini_path);
        s.controller_pick_player = -1;
      };
      spec.rows.push_back(dev_row);
    }

    // Footer: CANCEL
    RowDefinition cancel_row;
    cancel_row.label = _("CANCEL");
    cancel_row.kind = RowKind::Footer;
    cancel_row.icon = RowIcon::Exit;
    cancel_row.on_select = [](ShellState &s, Config &, const ShellActions &) {
      s.controller_pick_player = -1;
    };
    spec.rows.push_back(cancel_row);
  }

  return spec;
}

} // namespace ui::panels