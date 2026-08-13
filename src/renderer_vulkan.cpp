#include "renderer.h"

#include <SDL.h>
#include <SDL_image.h>

#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include <stb_image_resize2.h>

#include <algorithm>
#include <cctype>
#include <cmath>

#include <imgui.h>
#include <imgui_impl_sdl2.h>
#include <imgui_impl_sdlrenderer2.h>

#include "app_discovery.h"   // para TileColor
#include "config.h"
#include "shell_state.h"
#include "shell_ui.h"

/* Rellena la silueta (alfa) del icono con el color de tinte.
   Ideal para SVG monocromos tipo simple-icons (negro sobre transparente). */
static void applyTint(SDL_Surface* surf, const TileColor& tint) {
    for (int y = 0; y < surf->h; ++y) {
        unsigned char* row =
            reinterpret_cast<unsigned char*>(surf->pixels) + y * surf->pitch;
        for (int x = 0; x < surf->w; ++x) {
            unsigned char* p = row + x * 4;   // RGBA32
            p[0] = tint.r;
            p[1] = tint.g;
            p[2] = tint.b;
            // p[3] (alfa) se conserva: la forma no cambia
        }
    }
}

class VulkanRenderer final : public Renderer {
public:
    bool init(SDL_Window* window, const Config& cfg) override {
        window_ = window;

        SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");

        renderer_ = SDL_CreateRenderer(
            window, -1,
            SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);

        if (!renderer_) {
            SDL_Log("SDL_CreateRenderer falló: %s", SDL_GetError());
            return false;
        }

        IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG | IMG_INIT_WEBP);

        int w = 0, h = 0;
        SDL_GetRendererOutputSize(renderer_, &w, &h);

        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;

        ImGui::StyleColorsDark();

        loadShellFonts(cfg, (float)h);

        // Verificar que el atlas se construyó
        unsigned char* pixels;
        int width, height;
        io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
        SDL_Log("[ludex] Font atlas: %dx%d, pixels=%p", width, height, (void*)pixels);

        ImGui_ImplSDL2_InitForSDLRenderer(window, renderer_);
        ImGui_ImplSDLRenderer2_Init(renderer_);

        ready_ = true;
        return true;
    }

    void beginFrame() override {
        if (!ready_) return;
        ImGui_ImplSDLRenderer2_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();
    }

    void drawShell(
        ShellState& state,       // <-- quitado const
        const Config& cfg,
        const ShellActions& actions
    ) override {
        if (!ready_) return;
        drawShellImGui(state, cfg, actions);
    }

    void endFrame() override {
        if (!ready_) return;

        ImGui::Render();

        SDL_SetRenderDrawColor(renderer_, 18, 18, 20, 255);
        SDL_RenderClear(renderer_);
        ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(), renderer_);
        SDL_RenderPresent(renderer_);
    }

    void shutdown() override {
        if (!ready_) return;
        ImGui_ImplSDLRenderer2_Shutdown();
        ImGui_ImplSDL2_Shutdown();
        ImGui::DestroyContext();
        if (renderer_) SDL_DestroyRenderer(renderer_);
        renderer_ = nullptr;
        ready_ = false;
    }

    void* loadTextureFromFile(
        const std::filesystem::path& path,
        int* out_w,
        int* out_h,
        int max_dim,
        const TileColor* tint    // <-- AÑADIDO
    ) override {
        SDL_Surface* surface = nullptr;

        std::string ext = path.extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(),
                       [](unsigned char c) { return std::tolower(c); });

        if (ext == ".svg") {
            int target = max_dim > 0 ? max_dim : 128;
            SDL_RWops* rw = SDL_RWFromFile(path.string().c_str(), "rb");
            if (rw) {
                surface = IMG_LoadSizedSVG_RW(rw, target, target);
                SDL_RWclose(rw);
            }
        } else {
            surface = IMG_Load(path.string().c_str());
        }

        if (!surface) {
            SDL_Log("No se pudo cargar %s: %s",
                    path.string().c_str(), IMG_GetError());
            return nullptr;
        }

        SDL_Surface* rgba =
            SDL_ConvertSurfaceFormat(surface, SDL_PIXELFORMAT_RGBA32, 0);
        SDL_FreeSurface(surface);
        if (!rgba) return nullptr;

        // <-- NUEVO: aplicar tinte ANTES del resize para que sea nítido
        if (tint) applyTint(rgba, *tint);

        SDL_Surface* final_surf = rgba;

        if (max_dim > 0 && (rgba->w > max_dim || rgba->h > max_dim)) {
            float s = (float)max_dim / (float)std::max(rgba->w, rgba->h);
            int dw = std::max(1, (int)std::lround(rgba->w * s));
            int dh = std::max(1, (int)std::lround(rgba->h * s));

            SDL_Surface* resized = SDL_CreateRGBSurfaceWithFormat(
                0, dw, dh, 32, SDL_PIXELFORMAT_RGBA32);

            if (resized) {
                bool ok = stbir_resize_uint8_srgb(
                    (const unsigned char*)rgba->pixels,
                    rgba->w, rgba->h, rgba->pitch,
                    (unsigned char*)resized->pixels,
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

        SDL_Texture* texture =
            SDL_CreateTextureFromSurface(renderer_, final_surf);
        if (texture) {
            SDL_SetTextureScaleMode(texture, SDL_ScaleModeLinear);
        }

        if (out_w) *out_w = final_surf->w;
        if (out_h) *out_h = final_surf->h;

        SDL_FreeSurface(final_surf);
        return texture;
    }

    void freeTexture(void* texture) override {
        if (texture) SDL_DestroyTexture((SDL_Texture*)texture);
    }

    void getOutputSize(int* w, int* h) override {
        SDL_GetRendererOutputSize(renderer_, w, h);
    }

private:
    SDL_Window* window_ = nullptr;
    SDL_Renderer* renderer_ = nullptr;
    bool ready_ = false;
};

std::unique_ptr<Renderer> createVulkanRenderer() {
    return std::make_unique<VulkanRenderer>();
}

