#pragma once

#include <filesystem>
#include <functional>
#include <memory>

struct SDL_Window;

struct App;
struct ShellState;

class Renderer {
public:
    virtual ~Renderer() = default;

    virtual bool init(SDL_Window* window) = 0;

    virtual void beginFrame() = 0;

    virtual void drawShell(
        const ShellState& state,
        const std::function<void(const App&)>& on_launch
    ) = 0;

    virtual void endFrame() = 0;

    virtual void shutdown() = 0;

    virtual void* loadTextureFromFile(
        const std::filesystem::path& path,
        int* out_w = nullptr,
        int* out_h = nullptr,
        int max_dim = 0          // 0 = no pre-escalar
    ) = 0;

    virtual void freeTexture(void* texture) = 0;
};

std::unique_ptr<Renderer> createVulkanRenderer();