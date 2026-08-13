#include "renderer.h"

#include <SDL.h>
#include <SDL_image.h>

// Pre-escalado de alta calidad en CPU.
// El IMPLEMENTATION debe estar en UNA sola translation unit: esta.
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include <stb_image_resize2.h>

#include <algorithm>
#include <cmath>

#include <imgui.h>
#include <imgui_impl_sdl2.h>
#include <imgui_impl_sdlrenderer2.h>

#include "shell_state.h"
#include "shell_ui.h"

class VulkanRenderer final : public Renderer {
public:
    bool init(SDL_Window* window) override {
        window_ = window;
        // "0" = nearest, "1" = linear, "2" = best
        SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "2");

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

        IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG | IMG_INIT_WEBP);

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
    void* loadTextureFromFile(
        const std::filesystem::path& path,
        int* out_w,
        int* out_h,
        int max_dim
    ) override {
        SDL_Surface* surface = IMG_Load(path.string().c_str());
        if (!surface) {
            SDL_Log("IMG_Load falló para %s: %s",
                    path.string().c_str(),
                    IMG_GetError());
            return nullptr;
        }

        // Normalizamos a RGBA32 para poder remuestrear con stb.
        SDL_Surface* rgba =
            SDL_ConvertSurfaceFormat(surface, SDL_PIXELFORMAT_RGBA32, 0);
        SDL_FreeSurface(surface);
        if (!rgba) {
            return nullptr;
        }

        SDL_Surface* final_surf = rgba;

        // Pre-escalado en CPU con filtro de alta calidad (equivalente al
        // smoothscale a 64x64 del Python, pero a 128 para más nitidez).
        // Así la GPU solo hace un escalado suave 128 -> ~110 px y no un
        // minificado 5:1 que el bilinear sin mipmaps no puede limpiar.
        if (max_dim > 0 && (rgba->w > max_dim || rgba->h > max_dim)) {
            float s = static_cast<float>(max_dim) /
                      static_cast<float>(std::max(rgba->w, rgba->h));
            int dw = std::max(1, static_cast<int>(std::lround(rgba->w * s)));
            int dh = std::max(1, static_cast<int>(std::lround(rgba->h * s)));

            SDL_Surface* resized = SDL_CreateRGBSurfaceWithFormat(
                0, dw, dh, 32, SDL_PIXELFORMAT_RGBA32);

            if (resized) {
                bool ok = stbir_resize_uint8_srgb(
                    static_cast<const unsigned char*>(rgba->pixels),
                    rgba->w, rgba->h, rgba->pitch,
                    static_cast<unsigned char*>(resized->pixels),
                    dw, dh, resized->pitch,
                    STBIR_RGBA);

                if (ok) {
                    SDL_FreeSurface(rgba);
                    final_surf = resized;
                } else {
                    SDL_FreeSurface(resized);
                }
            }
        }

        SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer_, final_surf);

        if (texture) {
            SDL_SetTextureScaleMode(texture, SDL_ScaleModeLinear);
        }

        if (out_w) *out_w = final_surf->w;
        if (out_h) *out_h = final_surf->h;

        SDL_FreeSurface(final_surf);
        return texture;
    }

    void freeTexture(void* texture) override {
        if (texture) {
            SDL_DestroyTexture(static_cast<SDL_Texture*>(texture));
        }
    }

private:
    SDL_Window* window_ = nullptr;
    SDL_Renderer* renderer_ = nullptr;
    bool ready_ = false;
};

std::unique_ptr<Renderer> createVulkanRenderer() {
    return std::make_unique<VulkanRenderer>();
}