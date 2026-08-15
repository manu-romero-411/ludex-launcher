
#pragma once

#include <filesystem>
#include "shell_actions.h"
#include <memory>

struct SDL_Window;

struct App;
struct Config;
struct ShellState;
// por:
struct TileColor;

class Renderer {
public:
    virtual ~Renderer() = default;

    virtual bool init(SDL_Window* window, const Config& cfg) = 0;

    virtual void beginFrame() = 0;

    virtual void drawShell(
        ShellState& state,             // <-- sin const
        const Config& cfg,
        const ShellActions& actions
    ) = 0;

    virtual void endFrame() = 0;
    virtual void shutdown() = 0;

    virtual void* loadTextureFromFile(
        const std::filesystem::path& path,
        int* out_w = nullptr,
        int* out_h = nullptr,
        int max_dim = 0,
        const TileColor* tint = nullptr,
        int cover_w = 0,
        int cover_h = 0
    ) = 0;

    virtual void freeTexture(void* texture) = 0;
    virtual void getOutputSize(int* w, int* h) = 0;
    virtual void presentBlackFrame() = 0;
};

std::unique_ptr<Renderer> createVulkanRenderer();