#include "renderer.h"

#include <SDL.h>

#include <imgui.h>
#include <imgui_impl_sdl2.h>
#include <imgui_impl_sdlrenderer2.h>

#include "shell_state.h"
#include "shell_ui.h"

class VulkanRenderer final : public Renderer {
public:
    bool init(SDL_Window* window) override {
        window_ = window;

        // SDL_RENDERER_ACCELERATED usa la GPU (Vulkan/OpenGL por debajo en Wayland)
        renderer_ = SDL_CreateRenderer(
            window,
            -1,
            SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC
        );

        if (!renderer_) {
            SDL_Log("SDL_CreateRenderer falló: %s", SDL_GetError());
            return false;
        }

        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        (void)io;
        
        // Habilitar navegación por teclado y mando en ImGui
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;

        ImGui::StyleColorsDark();

        ImGui_ImplSDL2_InitForSDLRenderer(window, renderer_);
        ImGui_ImplSDLRenderer2_Init(renderer_);

        ready_ = true;
        return true;
    }

    void beginFrame() override {
        if (!ready_) {
            return;
        }

        ImGui_ImplSDLRenderer2_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();
    }

    void drawShell(
        const ShellState& state,
        const std::function<void(const App&)>& on_launch
    ) override {
        if (!ready_) {
            return;
        }

        drawShellImGui(state, on_launch);
    }

    void endFrame() override {
        if (!ready_) {
            return;
        }

        ImGui::Render();

        // Color de fondo fallback (gris oscuro)
        SDL_SetRenderDrawColor(renderer_, 18, 18, 20, 255);
        SDL_RenderClear(renderer_);

        // Dibujar la UI de ImGui
        ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(), renderer_);

        // Presentar en pantalla
        SDL_RenderPresent(renderer_);
    }

    void shutdown() override {
        if (!ready_) {
            return;
        }

        ImGui_ImplSDLRenderer2_Shutdown();
        ImGui_ImplSDL2_Shutdown();
        ImGui::DestroyContext();

        if (renderer_) {
            SDL_DestroyRenderer(renderer_);
            renderer_ = nullptr;
        }

        ready_ = false;
    }

private:
    SDL_Window* window_ = nullptr;
    SDL_Renderer* renderer_ = nullptr;
    bool ready_ = false;
};

std::unique_ptr<Renderer> createVulkanRenderer() {
    return std::make_unique<VulkanRenderer>();
}