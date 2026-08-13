#include "renderer.h"
#define STB_IMAGE_IMPLEMENTATION
#include <SDL.h>
#include <SDL_image.h>
#include <stb_image.h>

#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include <stb_image_resize2.h>

#include <algorithm>
#include <cctype>
#include <cmath>

#include <imgui.h>
#include <imgui_impl_sdl2.h>
#include <imgui_impl_sdlrenderer2.h>

#include "app_discovery.h" // para TileColor
#include "config.h"
#include "shell_state.h"
#include "shell_ui.h"
#ifdef LUDEX_HAVE_RSVG
#include <cairo.h>
#include <librsvg/rsvg.h>

static SDL_Surface *loadSvgSurface(const std::filesystem::path &path, int tw,
                                   int th) {
  GError *err = nullptr;
  RsvgHandle *handle = rsvg_handle_new_from_file(path.c_str(), &err);
  if (!handle) {
    if (err) {
      SDL_Log("rsvg: %s: %s", path.string().c_str(), err->message);
      g_error_free(err);
    }
    return nullptr;
  }

  cairo_surface_t *cs = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, tw, th);
  cairo_t *cr = cairo_create(cs);

  // SIN transforms manuales: librsvg escala y centra el documento
  // dentro del viewport (honra preserveAspectRatio).
  RsvgRectangle viewport{0.0, 0.0, (double)tw, (double)th};
  gboolean ok = rsvg_handle_render_document(handle, cr, &viewport, nullptr);

  cairo_destroy(cr);
  g_object_unref(handle);

  if (!ok || cairo_surface_status(cs) != CAIRO_STATUS_SUCCESS) {
    cairo_surface_destroy(cs);
    return nullptr;
  }

  int w = cairo_image_surface_get_width(cs);
  int h = cairo_image_surface_get_height(cs);
  int stride = cairo_image_surface_get_stride(cs);
  const unsigned char *src = cairo_image_surface_get_data(cs);

  // Cairo ARGB32 (premultiplicado, bytes B,G,R,A) -> SDL ARGB8888
  // con alfa recto (des-premultiplicado).
  SDL_Surface *surf =
      SDL_CreateRGBSurfaceWithFormat(0, w, h, 32, SDL_PIXELFORMAT_ARGB8888);
  if (!surf) {
    cairo_surface_destroy(cs);
    return nullptr;
  }

  for (int y = 0; y < h; ++y) {
    unsigned char *dst = (unsigned char *)surf->pixels + y * surf->pitch;
    const unsigned char *row = src + y * stride;
    for (int x = 0; x < w; ++x) {
      unsigned int b = row[x * 4 + 0], g = row[x * 4 + 1], r = row[x * 4 + 2],
                   a = row[x * 4 + 3];
      if (a > 0 && a < 255) {
        r = std::min(255u, (r * 255 + a / 2) / a);
        g = std::min(255u, (g * 255 + a / 2) / a);
        b = std::min(255u, (b * 255 + a / 2) / a);
      }
      dst[x * 4 + 0] = (unsigned char)b;
      dst[x * 4 + 1] = (unsigned char)g;
      dst[x * 4 + 2] = (unsigned char)r;
      dst[x * 4 + 3] = (unsigned char)a;
    }
  }

  cairo_surface_destroy(cs);
  return surf;
}
#endif // LUDEX_HAVE_RSVG

/* Rellena la silueta (alfa) del icono con el color de tinte.
   Ideal para SVG monocromos tipo simple-icons (negro sobre transparente). */
static void applyTint(SDL_Surface *surf, const TileColor &tint) {
  for (int y = 0; y < surf->h; ++y) {
    unsigned char *row =
        reinterpret_cast<unsigned char *>(surf->pixels) + y * surf->pitch;
    for (int x = 0; x < surf->w; ++x) {
      unsigned char *p = row + x * 4; // RGBA32
      p[0] = tint.r;
      p[1] = tint.g;
      p[2] = tint.b;
      // p[3] (alfa) se conserva: la forma no cambia
    }
  }
}

class VulkanRenderer final : public Renderer {
public:
  bool init(SDL_Window *window, const Config &cfg) override {
    window_ = window;

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");

    renderer_ = SDL_CreateRenderer(
        window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);

    if (!renderer_) {
      SDL_Log("SDL_CreateRenderer falló: %s", SDL_GetError());
      return false;
    }

    IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG | IMG_INIT_WEBP);

    int w = 0, h = 0;
    SDL_GetRendererOutputSize(renderer_, &w, &h);

    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    ImGui::StyleColorsDark();

    loadShellFonts(cfg, (float)h);

    // Verificar que el atlas se construyó
    unsigned char *pixels;
    int width, height;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
    SDL_Log("[ludex] Font atlas: %dx%d, pixels=%p", width, height,
            (void *)pixels);

    ImGui_ImplSDL2_InitForSDLRenderer(window, renderer_);
    ImGui_ImplSDLRenderer2_Init(renderer_);

    ready_ = true;
    return true;
  }

  void beginFrame() override {
    if (!ready_)
      return;
    ImGui_ImplSDLRenderer2_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();
  }

  void drawShell(ShellState &state, // <-- quitado const
                 const Config &cfg, const ShellActions &actions) override {
    if (!ready_)
      return;
    drawShellImGui(state, cfg, actions);
  }

  void endFrame() override {
    if (!ready_)
      return;

    ImGui::Render();

    SDL_SetRenderDrawColor(renderer_, 18, 18, 20, 255);
    SDL_RenderClear(renderer_);
    ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(), renderer_);
    SDL_RenderPresent(renderer_);
  }

  void shutdown() override {
    if (!ready_)
      return;
    ImGui_ImplSDLRenderer2_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
    if (renderer_)
      SDL_DestroyRenderer(renderer_);
    renderer_ = nullptr;
    ready_ = false;
  }

  void *loadTextureFromFile(const std::filesystem::path &path, int *out_w,
                            int *out_h, int max_dim, const TileColor *tint,
                            int cover_w, int cover_h) override {
    SDL_Surface *surface = nullptr;

    std::string ext = path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    if (ext == ".svg") {
      int target = max_dim > 0 ? max_dim : 128;
#ifdef LUDEX_HAVE_RSVG
      surface = loadSvgSurface(path, target, target);
      if (surface) {
        SDL_Log("[ludex] SVG via librsvg: %s", path.string().c_str());
      }
#endif
      if (!surface) {
        SDL_Log("[ludex] SVG via nanosvg (FALLBACK): %s",
                path.string().c_str());
        SDL_RWops *rw = SDL_RWFromFile(path.string().c_str(), "rb");
        if (rw) {
          surface = IMG_LoadSizedSVG_RW(rw, target, target);
          SDL_RWclose(rw);
        }
      }
    } else {
      // Primario: stb_image (determinista, sin dependencias externas)
      int iw = 0, ih = 0, comp = 0;
      unsigned char *data =
          stbi_load(path.string().c_str(), &iw, &ih, &comp, 4);
      if (data) {
        SDL_Surface *tmp = SDL_CreateRGBSurfaceWithFormatFrom(
            data, iw, ih, 32, iw * 4, SDL_PIXELFORMAT_RGBA32);
        if (tmp) {
          surface = SDL_ConvertSurfaceFormat(tmp, SDL_PIXELFORMAT_RGBA32, 0);
          SDL_FreeSurface(tmp);
        }
        stbi_image_free(data);
      }
      // Fallback: SDL_image (necesario para WEBP)
      if (!surface) {
        SDL_ClearError();
        surface = IMG_Load(path.string().c_str());
      }
    }

    if (!surface) {
      SDL_Log("No se pudo cargar %s: %s", path.string().c_str(),
              IMG_GetError());
      return nullptr;
    }

    SDL_Surface *rgba =
        SDL_ConvertSurfaceFormat(surface, SDL_PIXELFORMAT_RGBA32, 0);
    SDL_FreeSurface(surface);
    if (!rgba)
      return nullptr;

    if (tint)
      applyTint(rgba, *tint);

    // Resize de alta calidad que sustituye `rgba` con seguridad
    // (libera el antiguo solo si el nuevo se creó bien).
    auto resizeTo = [&](int dw, int dh) {
      if (dw == rgba->w && dh == rgba->h)
        return;
      SDL_Surface *resized =
          SDL_CreateRGBSurfaceWithFormat(0, dw, dh, 32, SDL_PIXELFORMAT_RGBA32);
      if (!resized)
        return;

      bool ok = stbir_resize_uint8_srgb(
          (const unsigned char *)rgba->pixels, rgba->w, rgba->h, rgba->pitch,
          (unsigned char *)resized->pixels, dw, dh, resized->pitch, STBIR_RGBA);

      if (ok) {
        SDL_FreeSurface(rgba);
        rgba = resized;
      } else {
        SDL_FreeSurface(resized);
      }
    };

    // 1) Cover a pantalla (wallpapers): ni un píxel de más en VRAM
    if (cover_w > 0 && cover_h > 0) {
      float s = std::max((float)cover_w / rgba->w, (float)cover_h / rgba->h);
      resizeTo(std::max(1, (int)std::lround(rgba->w * s)),
               std::max(1, (int)std::lround(rgba->h * s)));
    }

    // 2) Tope por dimensión máxima (iconos)
    if (max_dim > 0 && (rgba->w > max_dim || rgba->h > max_dim)) {
      float s = (float)max_dim / (float)std::max(rgba->w, rgba->h);
      resizeTo(std::max(1, (int)std::lround(rgba->w * s)),
               std::max(1, (int)std::lround(rgba->h * s)));
    }

    SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer_, rgba);
    if (texture) {
      SDL_SetTextureScaleMode(texture, SDL_ScaleModeLinear);
    } else {
      SDL_Log("SDL_CreateTextureFromSurface falló para %s: %s",
              path.string().c_str(), SDL_GetError());
    }

    if (out_w)
      *out_w = rgba->w;
    if (out_h)
      *out_h = rgba->h;

    SDL_FreeSurface(rgba); // la textura ya está en GPU
    return texture;
  }

  void freeTexture(void *texture) override {
    if (texture)
      SDL_DestroyTexture((SDL_Texture *)texture);
  }

  void getOutputSize(int *w, int *h) override {
    SDL_GetRendererOutputSize(renderer_, w, h);
  }

private:
  SDL_Window *window_ = nullptr;
  SDL_Renderer *renderer_ = nullptr;
  bool ready_ = false;
};

std::unique_ptr<Renderer> createVulkanRenderer() {
  return std::make_unique<VulkanRenderer>();
}
