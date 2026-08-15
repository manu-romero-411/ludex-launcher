#include "texture_handle.h"
#include "renderer.h"

void TextureDeleter::operator()(void* p) const {
    if (p && renderer) {
        renderer->freeTexture(p);
    }
    // Si renderer es null, la textura se pierde (leak menor)
    // pero no hay segfault.
}