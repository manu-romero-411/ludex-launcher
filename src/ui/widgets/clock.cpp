#include "clock.h"
#include "ui/ui_common.h"
#include <ctime>
#include "core/i18n.h"

namespace ui::widgets {

void Clock::draw(const Config& cfg, const ImGuiViewport* vp, bool left_side, bool bottom) {
    std::time_t now = std::time(nullptr);
    std::tm* local = std::localtime(&now);
    char time_text[32], date_text[128];
    std::strftime(time_text, sizeof(time_text), "%H:%M:%S", local);
    std::strftime(date_text, sizeof(date_text), _("%A %d %B %Y"), local);

    float margin = vp->WorkSize.x * 0.018f;

    ImGui::PushFont(ui::g_font_clock);
    ImVec2 ts = ImGui::CalcTextSize(time_text);
    ImGui::PopFont();
    ImGui::PushFont(ui::g_font_date);
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

    ImGui::PushFont(ui::g_font_clock);
    ImGui::SetCursorScreenPos(ImVec2(x_time, y_time));
    ImGui::TextUnformatted(time_text);
    ImGui::PopFont();

    ImGui::PushFont(ui::g_font_date);
    ImGui::SetCursorScreenPos(ImVec2(x_date, y_date));
    ImGui::TextColored(ImVec4(0.85f, 0.85f, 0.88f, 0.9f), "%s", date_text);
    ImGui::PopFont();
}

} // namespace ui::widgets