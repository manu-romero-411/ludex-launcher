#pragma once
#include <memory>

struct Renderer; // forward declaration

struct TextureDeleter {
    Renderer* renderer = nullptr;
    void operator()(void* p) const;
};

using TexturePtr = std::unique_ptr<void, TextureDeleter>;