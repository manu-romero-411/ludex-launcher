#include "assets.h"

#include <SDL.h>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <vector>

#include "renderer.h"
#include "shell_state.h"

static bool hasAllowedExt(
    const std::filesystem::path& path,
    const std::vector<std::string>& exts
) {
    std::string ext = path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return std::find(exts.begin(), exts.end(), ext) != exts.end();
}

void loadAllWallpapers(Renderer& renderer, ShellState& state,
                       const Config& cfg, int screen_w, int screen_h)
{
    // Liberar lo anterior
    for (auto& L : state.wallpapers) {
        if (L.texture) renderer.freeTexture(L.texture);
    }
    state.wallpapers.clear();
    state.wp_current = -1;
    state.wp_next = -1;
    state.wp_in_transition = false;
    state.wp_fade = 1.0f;
    state.wp_timer = 0.0f;

    std::error_code ec;
    if (!std::filesystem::is_directory(cfg.wallpaper_dir, ec)) {
        SDL_Log("[ludex] wallpaper_dir no es un directorio: %s",
                cfg.wallpaper_dir.string().c_str());
        return;
    }

    std::vector<std::filesystem::path> candidates;
    for (const auto& e : std::filesystem::directory_iterator(cfg.wallpaper_dir, ec)) {
        if (e.is_regular_file(ec) && hasAllowedExt(e.path(), cfg.wallpaper_exts)) {
            candidates.push_back(e.path());
        }
    }
    std::sort(candidates.begin(), candidates.end());

    SDL_Log("[ludex] %d candidatos de wallpaper en %s",
            (int)candidates.size(), cfg.wallpaper_dir.string().c_str());

    for (const auto& path : candidates) {
        WallpaperLayer L;
        L.texture = renderer.loadTextureFromFile(
            path, &L.w, &L.h,
            0, nullptr,
            screen_w, screen_h);   // cover a pantalla: VRAM justa
        if (L.texture) {
            L.kb_scale = 1.0f;
            L.kb_pan_x = 0.0f;
            L.kb_pan_y = 0.0f;
            state.wallpapers.push_back(L);
        }
    }

    if (!state.wallpapers.empty()) {
        state.wp_current = 0;
    }
    SDL_Log("[ludex] %d wallpapers cargados (pantalla %dx%d)",
            (int)state.wallpapers.size(), screen_w, screen_h);
}

void loadShellAssets(Renderer& renderer, ShellState& state,
                     const Config& cfg, int screen_w, int screen_h)
{
    // Iconos de apps
    int icon_max = (int)(screen_h * cfg.icon_sel_pct * 2.0f);

    for (auto& app : state.apps) {
        if (app.icon_texture) {
            renderer.freeTexture(app.icon_texture);
            app.icon_texture = nullptr;
        }
        if (!app.icon_path.empty()) {
            app.icon_texture = renderer.loadTextureFromFile(
                app.icon_path, nullptr, nullptr, icon_max,
                app.has_icon_tint ? &app.icon_tint : nullptr);
        }
    }

    // >>> ESTA llamada es la que faltaba en tu archivo <<<
    loadAllWallpapers(renderer, state, cfg, screen_w, screen_h);
}

void freeShellAssets(Renderer& renderer, ShellState& state) {
    for (auto& app : state.apps) {
        if (app.icon_texture) {
            renderer.freeTexture(app.icon_texture);
            app.icon_texture = nullptr;
        }
    }
    for (auto& L : state.wallpapers) {
        if (L.texture) renderer.freeTexture(L.texture);
    }
    state.wallpapers.clear();
    state.wp_current = -1;
    state.wp_next = -1;
}