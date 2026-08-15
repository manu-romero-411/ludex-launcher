#pragma once

#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>
#include "texture_handle.h"

struct Renderer;
struct TileColor;

/// Caché LRU de iconos de apps. Carga texturas bajo demanda y evita
/// mantener en VRAM iconos que no están en pantalla.
class IconCache {
public:
    /// Configura el renderer y el tamaño máximo de icono.
    void configure(Renderer *renderer, int max_dim);

    /// Devuelve la textura para un icono, cargándola si no está en caché.
    /// Devuelve nullptr si la ruta está vacía o la carga falla.
    void *get(const std::filesystem::path &path, const TileColor *tint);

    /// Marca el inicio de un nuevo frame. Incrementa el contador interno.
    void beginFrame();

    /// Libera texturas que llevan más de `max_age` frames sin usarse.
    void evict(int max_age = 4);

    /// Libera todas las texturas.
    void clear();

    /// Número de texturas actualmente en caché.
    size_t size() const { return cache_.size(); }

private:
    struct Entry {
        TexturePtr tex;
        int last_used_frame = 0;
    };

    std::unordered_map<std::string, Entry> cache_;
    Renderer *renderer_ = nullptr;
    int max_dim_ = 128;
    int frame_ = 0;
};