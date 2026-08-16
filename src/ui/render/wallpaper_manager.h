#pragma once
#include <atomic>
#include <condition_variable>
#include <filesystem>
#include <mutex>
#include <thread>
#include <vector>
#include <imgui.h>
#include "renderer.h"
#include "texture_handle.h"

struct Config;

/// Capa de wallpaper con estado Ken Burns.
struct WallpaperLayer {
    TexturePtr texture;
    int w = 0, h = 0;
    float kb_scale = 1.0f;
    float kb_pan_x = 0.0f;
    float kb_pan_y = 0.0f;
};

/// Gestiona wallpapers con carga diferida y decode asíncrono:
/// - En VRAM solo viven 2 texturas (actual + siguiente en crossfade).
/// - El decode/resize del siguiente wallpaper ocurre en un hilo de trabajo;
///   el main thread solo sube a GPU y anima. La rotación no congela la UI.
class WallpaperManager {
public:
    WallpaperManager() = default;
    ~WallpaperManager();

    WallpaperManager(const WallpaperManager &) = delete;
    WallpaperManager &operator=(const WallpaperManager &) = delete;

    /// Escanea el directorio y guarda las rutas (sin cargar texturas).
    void discover(const Config &cfg);
    /// Carga un wallpaper aleatorio inicial (síncrono: debe estar ya).
    void loadInitial(Renderer &r, int screen_w, int screen_h);
    /// Fuerza la transición al siguiente (tecla W). Arranca al terminar
    /// el decode asíncrono.
    void forceNext(Renderer &r, int screen_w, int screen_h);
    /// Timers, Ken Burns, crossfade y consumo del decode asíncrono.
    void update(float dt, const Config &cfg, Renderer &r,
                int screen_w, int screen_h);
    /// Dibuja el wallpaper actual (y el siguiente si hay transición).
    void draw(ImDrawList *dl, float W, float H, const Config &cfg) const;
    /// Libera texturas y descarta trabajo pendiente.
    void clear();
    bool hasWallpaper() const { return current_.texture != nullptr; }

private:
    struct PendingRequest {
        std::filesystem::path path;
        int cover_w = 0, cover_h = 0;
    };

    void workerLoop();
    void beginNext(int screen_w, int screen_h);
    void finishTransition();
    int pickRandomIndex() const;

    std::vector<std::filesystem::path> paths_;
    WallpaperLayer current_;
    WallpaperLayer next_;
    int current_idx_ = -1;
    int next_idx_ = -1;
    float timer_ = 0.0f;
    float fade_ = 1.0f;
    bool in_transition_ = false;

    // ---- estado del decode asíncrono (protegido por mtx_) ----
    std::thread worker_;
    mutable std::mutex mtx_;
    std::condition_variable cv_;
    bool stop_ = false;
    bool has_request_ = false;
    PendingRequest request_;
    bool result_ready_ = false;
    bool result_ok_ = false;
    DecodedImage result_;
    std::atomic<Renderer *> renderer_{nullptr};
};