#include "shell_ui.h"

#include <imgui.h>

#include <cmath>
#include <ctime>
#include <string>

static void drawClock(const ImGuiViewport* viewport) {
    std::time_t now = std::time(nullptr);
    std::tm* local = std::localtime(&now);

    char time_text[32];
    char date_text[128];

    std::strftime(time_text, sizeof(time_text), "%H:%M", local);
    std::strftime(date_text, sizeof(date_text), "%A %d %B", local);

    ImVec2 time_size = ImGui::CalcTextSize(time_text);
    ImVec2 date_size = ImGui::CalcTextSize(date_text);

    float margin = 34.0f;

    ImGui::SetCursorPos(
        ImVec2(
            viewport->WorkSize.x - margin - time_size.x,
            margin
        )
    );

    ImGui::TextUnformatted(time_text);

    ImGui::SetCursorPos(
        ImVec2(
            viewport->WorkSize.x - margin - date_size.x,
            margin + time_size.y + 4.0f
        )
    );

    ImGui::TextColored(ImVec4(0.75f, 0.75f, 0.77f, 1.0f), "%s", date_text);
}

void drawShellImGui(
    const ShellState& state,
    const std::function<void(const App&)>& on_launch
) {
    const ImGuiViewport* viewport = ImGui::GetMainViewport();

    // ---- fondo (wallpaper con cover-crop + overlay) ----
    ImDrawList* bg = ImGui::GetBackgroundDrawList();
    const ImVec2 screen = viewport->WorkSize;

    if (state.wallpaper_texture && state.wallpaper_w > 0 && state.wallpaper_h > 0) {
        float sw = screen.x;
        float sh = screen.y;
        float iw = static_cast<float>(state.wallpaper_w);
        float ih = static_cast<float>(state.wallpaper_h);

        // background-size: cover
        float scale = std::max(sw / iw, sh / ih);
        float vis_w = sw / scale;
        float vis_h = sh / scale;

        ImVec2 uv0((iw - vis_w) * 0.5f / iw, (ih - vis_h) * 0.5f / ih);
        ImVec2 uv1(uv0.x + vis_w / iw, uv0.y + vis_h / ih);

        bg->AddImage(
            static_cast<ImTextureID>(state.wallpaper_texture),
            ImVec2(0.0f, 0.0f),
            screen,
            uv0,
            uv1
        );

        // OVERLAY_ALPHA = 90, igual que en el Python
        bg->AddRectFilled(
            ImVec2(0.0f, 0.0f),
            screen,
            IM_COL32(0, 0, 0, 90)
        );
    } else {
        bg->AddRectFilled(
            ImVec2(0.0f, 0.0f),
            screen,
            IM_COL32(18, 18, 20, 255)
        );
    }

    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);

    ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_NoBackground |
        ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoScrollWithMouse |
        ImGuiWindowFlags_NoScrollbar;

    ImGui::Begin("##ludex-launcher", nullptr, flags);

    drawClock(viewport);

    if (state.apps.empty()) {
        ImGui::SetCursorPos(ImVec2(40.0f, viewport->WorkSize.y - 220.0f));
        ImGui::TextUnformatted("Sin apps en APPS_DIR");
        ImGui::End();
        return;
    }

    const float base_tile_w = 170.0f;
    const float base_tile_h = 130.0f;
    const float spacing = 40.0f;
    const float bottom_margin = 170.0f;
    const float selected_scale = 1.35f;

    float center_x = viewport->WorkSize.x * 0.5f;
    float center_y = viewport->WorkSize.y - bottom_margin;

    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    for (int i = 0; i < static_cast<int>(state.apps.size()); ++i) {
        const App& app = state.apps[i];

        float dist = static_cast<float>(i) - state.carousel_offset;

        float scale = std::max(
            1.0f - std::fabs(dist) * 0.18f,
            selected_scale - std::fabs(dist) * 0.35f
        );

        if (scale < 0.55f) {
            scale = 0.55f;
        }

        float tile_w = base_tile_w * scale;
        float tile_h = base_tile_h * scale;

        float x = center_x + dist * (base_tile_w + spacing) - tile_w * 0.5f;
        float y = center_y - tile_h * 0.5f;

        ImGui::PushID(i);

        ImGui::SetCursorPos(ImVec2(x, y));

        if (ImGui::InvisibleButton("##tile", ImVec2(tile_w, tile_h))) {
            if (on_launch) {
                on_launch(app);
            }
        }

        ImVec2 min = ImGui::GetItemRectMin();
        ImVec2 max = ImGui::GetItemRectMax();

        bool selected = (i == state.selected);

        ImU32 color = selected
            ? IM_COL32(90, 140, 220, 245)
            : IM_COL32(40, 40, 46, 235);

        draw_list->AddRectFilled(min, max, color, 16.0f);

        // ---- icono de la app ----
        if (app.icon_texture) {
            float icon_size = tile_w * 0.5f;

            ImVec2 center(
                min.x + tile_w * 0.5f,
                min.y + tile_h * 0.5f - 10.0f
            );

            draw_list->AddImage(
                static_cast<ImTextureID>(app.icon_texture),
                ImVec2(center.x - icon_size * 0.5f, center.y - icon_size * 0.5f),
                ImVec2(center.x + icon_size * 0.5f, center.y + icon_size * 0.5f)
            );
        }
        // ---- fin icono ----
        ImVec2 text_size = ImGui::CalcTextSize(app.name.c_str());

        if (selected) {
            ImVec2 text_pos(
                center_x - text_size.x * 0.5f,
                center_y + tile_h * 0.5f + 24.0f
            );

            draw_list->AddText(
                text_pos,
                IM_COL32(235, 235, 235, 255),
                app.name.c_str()
            );
        } else {
            ImVec2 text_pos(
                min.x + (tile_w - text_size.x) * 0.5f,
                min.y + tile_h + 6.0f
            );

            draw_list->AddText(
                text_pos,
                IM_COL32(190, 190, 195, 220),
                app.name.c_str()
            );
        }

        ImGui::PopID();
    }

    ImGui::End();
}
