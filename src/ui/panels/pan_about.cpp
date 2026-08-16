#include "pan_about.h"
#include "core/system_info.h"
#include "core/i18n.h"

namespace ui::panels {

PanelSpec makeAboutPanelSpec(ShellState &st, const ShellActions &actions) {
    PanelSpec spec;
    spec.title = _("ABOUT");
    spec.focus_ptr = &st.about_focus;
    spec.scroll_ptr = &st.about_scroll;
    spec.on_back = [](ShellState &s, Config &) {
        s.show_about = false;
        s.show_system = true;
        s.system_focus = 0;
    };

    SystemInfo si = SystemInfo::collect();

    auto addRow = [&](const std::string &label, const std::string &value,
                      RowIcon icon) {
        RowDefinition row;
        row.kind = RowKind::Label;
        row.label = label;
        row.icon = icon;
        row.get_value = [value](const Config &) { return value; };
        spec.rows.push_back(row);
    };

    addRow(_("PROCESSOR"),   si.cpu,        RowIcon::Cpu);
    addRow(_("GRAPHICS"),    si.gpu,        RowIcon::Gpu);
    addRow(_("MEMORY"),      si.ram,        RowIcon::Ram);
    addRow(_("VRAM"),        si.vram,       RowIcon::Vram);
    addRow(_("OPERATING SYSTEM"), si.os,    RowIcon::Os);
    addRow(_("DESKTOP"),     si.desktop,    RowIcon::Desktop);
    addRow(_("LUDEX COMMIT"), si.git_commit, RowIcon::GitCommit);
    addRow(_("BUILT ON"),    si.build_date, RowIcon::BuildDate);

    RowDefinition back_row;
    back_row.label = _("BACK");
    back_row.kind = RowKind::Footer;
    back_row.icon = RowIcon::Exit;
    back_row.on_select = [](ShellState &s, Config &, const ShellActions &) {
        s.show_about = false;
        s.show_system = true;
        s.system_focus = 0;
    };
    spec.rows.push_back(back_row);
    return spec;
}

} // namespace ui::panels