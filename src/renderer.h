#pragma once

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
};

std::unique_ptr<Renderer> createVulkanRenderer();
