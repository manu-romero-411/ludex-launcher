#pragma once
#include <imgui.h>
#include "config.h"
#include "shell_state.h"

namespace ui::panels {

struct PanelLayout {
    ImVec2 panel_min, panel_max;
    ImVec2 content_min, content_max;
    float title_h, row_h, footer_h, pad;
    float pw, ph, px, py;
    ImU32 fade_col, panel_bg, panel_title;
    ImU32 row_focus, row_hover, row_pressed;
    ImU32 text_main, text_val, border_col;

    ImVec2 rowMin(int i) const;
    ImVec2 rowMax(int i) const;
    ImVec2 footerButtonMin(int f, int footer_rows, int list_count) const;
    ImVec2 footerButtonMax(int f, int footer_rows, int list_count) const;
};

class PanelBase {
public:
    virtual ~PanelBase() = default;
    
protected:
    // Datos de una fila del panel
    struct RowData {
        const char* label;
        bool adjustable;
        std::string value;
        RowIcon icon;
    };

    // Inicializa el layout y dibuja el fondo/título
    PanelLayout beginPanel(const char* title, int list_count, int footer_rows, const Config& cfg);
    
    // Dibuja una fila interactiva
    void drawRow(int id, const RowData& row, const PanelLayout& L, bool focused, bool hovered);
    
    // Dibuja un botón del footer
    void drawFooterButton(int id, const char* label, const PanelLayout& L, 
                          int button_index, int total_buttons, int list_count,
                          bool focused, bool hovered);
};

} // namespace ui::panels