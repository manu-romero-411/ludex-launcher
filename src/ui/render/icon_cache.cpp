#include "icon_cache.h"
#include "renderer.h"
#include "../../core/app_discovery.h" // TileColor
#include <SDL.h>

void IconCache::configure(Renderer *renderer, int max_dim) {
    renderer_ = renderer;
    max_dim_ = max_dim;
}

void *IconCache::get(const std::filesystem::path &path, const TileColor *tint) {
    if (path.empty() || !renderer_)
        return nullptr;

    std::string key = path.string();
    if (tint) {
        key += "|tint:" + std::to_string(tint->r) + "," +
               std::to_string(tint->g) + "," + std::to_string(tint->b);
    }

    auto it = cache_.find(key);
    if (it != cache_.end()) {
        it->second.last_used_frame = frame_;
        return it->second.tex.get();
    }

    void *raw = renderer_->loadTextureFromFile(path, nullptr, nullptr,
                                               max_dim_, tint);
    if (!raw) {
        SDL_Log("[ludex] IconCache: fallo al cargar %s", path.string().c_str());
        return nullptr;
    }

    TexturePtr tex(raw, TextureDeleter{renderer_});
    void *ptr = tex.get();
    cache_.emplace(key, Entry{std::move(tex), frame_});
    return ptr;
}

void IconCache::beginFrame() {
    ++frame_;
}

void IconCache::evict(int max_age) {
    for (auto it = cache_.begin(); it != cache_.end();) {
        if (frame_ - it->second.last_used_frame > max_age) {
            it = cache_.erase(it);
        } else {
            ++it;
        }
    }
}

void IconCache::clear() {
    cache_.clear();
}