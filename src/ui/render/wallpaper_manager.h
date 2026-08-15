#pragma once

#include <filesystem>
#include <vector>
#include <imgui.h>
#include "texture_handle.h"

struct Renderer;
struct Config;

/// Capa de wallpaper con estado Ken Burns.
struct WallpaperLayer {
    TexturePtr texture;
    int w = 0, h = 0;
    float kb_scale = 1.0f;
    float kb_pan_x = 0.0f;
    float kb_pan_y = 0.0f;
};

/// Gestiona wallpapers con carga diferida: solo mantiene en VRAM el
/// wallpaper actual y, durante el crossfade, el siguiente.
class WallpaperManager {
public:
    /// Escanea el directorio y guarda las rutas (sin cargar texturas).
    void discover(const Config &cfg);

    /// Carga un wallpaper aleatorio inicial.
    void loadInitial(Renderer &r, int screen_w, int screen_h);

    /// Fuerza la transición al siguiente wallpaper (tecla W).
    void forceNext(Renderer &r, int screen_w, int screen_h);

    /// Actualiza timers, Ken Burns y crossfade. Carga el siguiente
    /// wallpaper cuando toca.
    void update(float dt, const Config &cfg, Renderer &r,
                int screen_w, int screen_h);

    /// Dibuja el wallpaper actual (y el siguiente si hay transición).
    void draw(ImDrawList *dl, float W, float H, const Config &cfg) const;

    /// Libera toda textura y estado.
    void clear();

    bool hasWallpaper() const { return current_.texture != nullptr; }

private:
    void startTransition(Renderer &r, int screen_w, int screen_h);
    void finishTransition();
    int pickRandomIndex() const;
    WallpaperLayer loadLayer(Renderer &r, const std::filesystem::path &path,
                             int screen_w, int screen_h);

    std::vector<std::filesystem::path> paths_;
    WallpaperLayer current_;
    WallpaperLayer next_;
    int current_idx_ = -1;
    int next_idx_ = -1;
    float timer_ = 0.0f;
    float fade_ = 1.0f;
    bool in_transition_ = false;
};