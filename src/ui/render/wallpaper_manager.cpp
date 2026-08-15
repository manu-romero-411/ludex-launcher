#include "wallpaper_manager.h"
#include "renderer.h"
#include "../../core/config.h"
#include "../widgets/wallpaper.h"
#include <SDL.h>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <random>

namespace {

bool hasAllowedExt(const std::filesystem::path &path,
                   const std::vector<std::string> &exts) {
    std::string ext = path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return std::find(exts.begin(), exts.end(), ext) != exts.end();
}

std::mt19937 &rng() {
    static std::mt19937 g{std::random_device{}()};
    return g;
}

} // namespace

void WallpaperManager::discover(const Config &cfg) {
    paths_.clear();
    std::error_code ec;
    if (!std::filesystem::is_directory(cfg.wallpaper_dir, ec)) {
        SDL_Log("[ludex] wallpaper_dir no es un directorio: %s",
                cfg.wallpaper_dir.string().c_str());
        return;
    }
    for (const auto &e :
         std::filesystem::directory_iterator(cfg.wallpaper_dir, ec)) {
        if (e.is_regular_file(ec) &&
            hasAllowedExt(e.path(), cfg.wallpaper_exts)) {
            paths_.push_back(e.path());
        }
    }
    std::sort(paths_.begin(), paths_.end());
    SDL_Log("[ludex] %d wallpapers descubiertos en %s",
            (int)paths_.size(), cfg.wallpaper_dir.string().c_str());
}

void WallpaperManager::loadInitial(Renderer &r, int screen_w, int screen_h) {
    clear();
    if (paths_.empty())
        return;
    current_idx_ = pickRandomIndex();
    current_ = loadLayer(r, paths_[current_idx_], screen_w, screen_h);
    timer_ = 0.0f;
    SDL_Log("[ludex] wallpaper inicial: %s",
            paths_[current_idx_].filename().string().c_str());
}

void WallpaperManager::forceNext(Renderer &r, int screen_w, int screen_h) {
    if (paths_.size() < 2 || in_transition_)
        return;
    startTransition(r, screen_w, screen_h);
}

void WallpaperManager::update(float dt, const Config &cfg, Renderer &r,
                              int screen_w, int screen_h) {
    if (!current_.texture)
        return;

    // Ken Burns en la capa actual
    if (cfg.wallpaper_ken_burns && !in_transition_) {
        float a = std::min(1.0f, 0.15f * dt);
        current_.kb_scale +=
            (cfg.wallpaper_ken_burns_zoom - current_.kb_scale) * a;
    }

    // Timer para rotación automática
    if (!in_transition_) {
        timer_ += dt;
        if (cfg.wallpaper_rotate && timer_ >= cfg.wallpaper_interval) {
            startTransition(r, screen_w, screen_h);
        }
    }

    // Crossfade en curso
    if (in_transition_) {
        if (cfg.wallpaper_ken_burns) {
            float a = std::min(1.0f, 0.15f * dt);
            next_.kb_scale +=
                (cfg.wallpaper_ken_burns_zoom - next_.kb_scale) * a;
        }
        float fade_speed = (cfg.wallpaper_fade_duration > 0.001f)
                               ? 1.0f / cfg.wallpaper_fade_duration
                               : 1000.0f;
        fade_ -= fade_speed * dt;
        if (fade_ <= 0.0f) {
            finishTransition();
        }
    }
}

void WallpaperManager::draw(ImDrawList *dl, float W, float H,
                            const Config &cfg) const {
    if (!current_.texture)
        return;

    bool light = isLight(cfg.theme);
    ImU32 overlay = light ? IM_COL32(255, 255, 255, 80)
                          : IM_COL32(0, 0, 0, 90);

    if (in_transition_ && next_.texture) {
        float raw = fade_;
        float f = raw * raw * (3.0f - 2.0f * raw); // smoothstep
        int a_cur = (int)(255 * f);
        int a_nxt = (int)(255 * (1.0f - f));
        ui::widgets::drawWallpaperLayer(dl, W, H, current_,
                           IM_COL32(255, 255, 255, a_cur));
        ui::widgets::drawWallpaperLayer(dl, W, H, next_,
                           IM_COL32(255, 255, 255, a_nxt));
    } else {
        ui::widgets::drawWallpaperLayer(dl, W, H, current_,
                           IM_COL32(255, 255, 255, 255));
    }
    dl->AddRectFilled(ImVec2(0, 0), ImVec2(W, H), overlay);
}

void WallpaperManager::clear() {
    current_ = {};
    next_ = {};
    current_idx_ = -1;
    next_idx_ = -1;
    timer_ = 0.0f;
    fade_ = 1.0f;
    in_transition_ = false;
}

// --- privado ---

void WallpaperManager::startTransition(Renderer &r, int screen_w,
                                       int screen_h) {
    if (paths_.size() < 2 || in_transition_)
        return;
    next_idx_ = pickRandomIndex();
    next_ = loadLayer(r, paths_[next_idx_], screen_w, screen_h);
    if (!next_.texture) {
        SDL_Log("[ludex] fallo cargando siguiente wallpaper");
        return;
    }
    // Estado inicial Ken Burns para la nueva capa
    next_.kb_scale = 1.0f;
    std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
    next_.kb_pan_x = dist(rng()) * 0.10f;
    next_.kb_pan_y = dist(rng()) * 0.10f;

    in_transition_ = true;
    fade_ = 1.0f;
}

void WallpaperManager::finishTransition() {
    current_ = std::move(next_);
    current_idx_ = next_idx_;
    next_ = {};
    next_idx_ = -1;
    in_transition_ = false;
    fade_ = 1.0f;
    timer_ = 0.0f;
}

int WallpaperManager::pickRandomIndex() const {
    if (paths_.empty())
        return -1;
    std::uniform_int_distribution<int> dist(0, (int)paths_.size() - 1);
    int idx = dist(rng());
    // Evitar repetir el actual si hay más de uno
    if (paths_.size() > 1 && idx == current_idx_) {
        idx = (idx + 1) % (int)paths_.size();
    }
    return idx;
}

WallpaperLayer WallpaperManager::loadLayer(Renderer &r,
                                           const std::filesystem::path &path,
                                           int screen_w, int screen_h) {
    WallpaperLayer L;
    int w = 0, h = 0;
    void *raw =
        r.loadTextureFromFile(path, &w, &h, 0, nullptr, screen_w, screen_h);
    if (raw) {
        L.texture = TexturePtr(raw, TextureDeleter{&r});
        L.w = w;
        L.h = h;
    }
    return L;
}