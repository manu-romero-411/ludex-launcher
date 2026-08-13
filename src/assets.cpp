#include "assets.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <random>
#include <vector>

#include "renderer.h"
#include "shell_state.h"

static bool hasAllowedExt(
    const std::filesystem::path& path,
    const std::vector<std::string>& exts
) {
    std::string ext = path.extension().string();

    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) {
        return std::tolower(c);
    });

    return std::find(exts.begin(), exts.end(), ext) != exts.end();
}

void loadWallpaper(Renderer& renderer, ShellState& state, const Config& cfg) {
    if (state.wallpaper_texture) {
        renderer.freeTexture(state.wallpaper_texture);
        state.wallpaper_texture = nullptr;
    }

    std::error_code ec;
    if (!std::filesystem::is_directory(cfg.wallpaper_dir, ec)) {
        return;
    }

    std::vector<std::filesystem::path> candidates;

    for (const auto& entry :
         std::filesystem::directory_iterator(cfg.wallpaper_dir, ec)) {
        if (entry.is_regular_file(ec) &&
            hasAllowedExt(entry.path(), cfg.wallpaper_exts)) {
            candidates.push_back(entry.path());
        }
    }

    if (candidates.empty()) {
        return;
    }

    // Equivalente al random.choice() del Python: baraja y prueba hasta
    // que una cargue.
    std::mt19937 rng(std::random_device{}());
    std::shuffle(candidates.begin(), candidates.end(), rng);

    for (const auto& path : candidates) {
        int w = 0;
        int h = 0;

        void* texture = renderer.loadTextureFromFile(path, &w, &h);
        if (texture) {
            state.wallpaper_texture = texture;
            state.wallpaper_w = w;
            state.wallpaper_h = h;
            return;
        }
    }
}

void loadShellAssets(Renderer& renderer, ShellState& state, const Config& cfg) {
    for (auto& app : state.apps) {
        if (app.icon_texture) {
            renderer.freeTexture(app.icon_texture);
            app.icon_texture = nullptr;
        }

        if (!app.icon_path.empty()) {
            app.icon_texture = renderer.loadTextureFromFile(
                app.icon_path,
                nullptr,
                nullptr,
                128   // tope en CPU; la GPU solo suaviza a partir de aquí
            );
        }
    }

    loadWallpaper(renderer, state, cfg);
}

void freeShellAssets(Renderer& renderer, ShellState& state) {
    for (auto& app : state.apps) {
        if (app.icon_texture) {
            renderer.freeTexture(app.icon_texture);
            app.icon_texture = nullptr;
        }
    }

    if (state.wallpaper_texture) {
        renderer.freeTexture(state.wallpaper_texture);
        state.wallpaper_texture = nullptr;
    }
}