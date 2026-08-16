#include "wallpaper_manager.h"
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

WallpaperManager::~WallpaperManager() {
    {
        std::lock_guard<std::mutex> l(mtx_);
        stop_ = true;
    }
    cv_.notify_all();
    if (worker_.joinable())
        worker_.join();
}

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
    renderer_.store(&r);

    // Arranca el worker la primera vez
    {
        std::lock_guard<std::mutex> l(mtx_);
        if (!worker_.joinable()) {
            stop_ = false;
            worker_ = std::thread([this] { workerLoop(); });
        }
    }

    clear();
    if (paths_.empty())
        return;

    // El wallpaper inicial se carga síncrono: debe estar listo ya.
    current_idx_ = pickRandomIndex();
    DecodedImage img;
    if (r.decodeImage(paths_[current_idx_], screen_w, screen_h, img)) {
        void *tex = r.createTextureFromPixels(img.pixels.data(), img.w, img.h);
        if (tex) {
            current_.texture = TexturePtr(tex, TextureDeleter{&r});
            current_.w = img.w;
            current_.h = img.h;
        }
    }
    timer_ = 0.0f;
    SDL_Log("[ludex] wallpaper inicial: %s",
            paths_[current_idx_].filename().string().c_str());
}

void WallpaperManager::forceNext(Renderer &r, int screen_w, int screen_h) {
    renderer_.store(&r);
    beginNext(screen_w, screen_h);
}

void WallpaperManager::beginNext(int screen_w, int screen_h) {
    if (paths_.size() < 2 || in_transition_ || next_idx_ >= 0)
        return;
    next_idx_ = pickRandomIndex();
    {
        std::lock_guard<std::mutex> l(mtx_);
        request_.path = paths_[next_idx_];
        request_.cover_w = screen_w;
        request_.cover_h = screen_h;
        has_request_ = true;
        result_ready_ = false; // descarta resultados obsoletos
    }
    cv_.notify_all();
}

void WallpaperManager::update(float dt, const Config &cfg, Renderer &r,
                              int screen_w, int screen_h) {
    if (!current_.texture)
        return;
    renderer_.store(&r);

    // Ken Burns en la capa actual
    if (cfg.wallpaper_ken_burns && !in_transition_) {
        float a = std::min(1.0f, 0.15f * dt);
        current_.kb_scale +=
            (cfg.wallpaper_ken_burns_zoom - current_.kb_scale) * a;
    }

    if (!in_transition_) {
        if (next_idx_ < 0) {
            // Sin nada pedido: cuenta el timer de rotación
            timer_ += dt;
            if (cfg.wallpaper_rotate && timer_ >= cfg.wallpaper_interval)
                beginNext(screen_w, screen_h);
        } else {
            // Decode en vuelo: consúmelo cuando esté listo
            bool ready = false, ok = false;
            DecodedImage img;
            {
                std::lock_guard<std::mutex> l(mtx_);
                if (result_ready_) {
                    ready = true;
                    ok = result_ok_;
                    img = std::move(result_);
                    result_ready_ = false;
                }
            }
            if (ready) {
                void *tex = nullptr;
                if (ok && img.valid())
                    tex = r.createTextureFromPixels(img.pixels.data(),
                                                    img.w, img.h);
                if (tex) {
                    next_.texture = TexturePtr(tex, TextureDeleter{&r});
                    next_.w = img.w;
                    next_.h = img.h;
                    next_.kb_scale = 1.0f;
                    std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
                    next_.kb_pan_x = dist(rng()) * 0.10f;
                    next_.kb_pan_y = dist(rng()) * 0.10f;
                    in_transition_ = true;
                    fade_ = 1.0f;
                } else {
                    next_idx_ = -1; // fallo: reintentará en el próximo ciclo
                }
            }
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
        if (fade_ <= 0.0f)
            finishTransition();
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
    {
        std::lock_guard<std::mutex> l(mtx_);
        has_request_ = false;
        result_ready_ = false;
    }
    current_ = {};
    next_ = {};
    current_idx_ = -1;
    next_idx_ = -1;
    timer_ = 0.0f;
    fade_ = 1.0f;
    in_transition_ = false;
}

// ---------------------------------------------------------------- privado --

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

void WallpaperManager::workerLoop() {
    for (;;) {
        PendingRequest req;
        {
            std::unique_lock<std::mutex> l(mtx_);
            cv_.wait(l, [this] { return stop_ || has_request_; });
            if (stop_)
                return;
            req = request_;
            has_request_ = false;
        }

        // Decode + resize en CPU, fuera del hilo principal
        DecodedImage img;
        bool ok = false;
        Renderer *r = renderer_.load();
        if (r)
            ok = r->decodeImage(req.path, req.cover_w, req.cover_h, img);

        {
            std::lock_guard<std::mutex> l(mtx_);
            if (stop_)
                return;
            result_ = std::move(img);
            result_ok_ = ok;
            result_ready_ = true;
        }
    }
}