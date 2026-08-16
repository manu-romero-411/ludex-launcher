
#pragma once

#pragma once
#include "ui/shell/shell_actions.h"
#include <cstdint>
#include <filesystem>
#include <memory>
#include <vector>

/// Imagen decodificada en CPU (RGBA32, pitch = w*4). Sin GPU.
struct DecodedImage {
  std::vector<uint8_t> pixels;
  int w = 0;
  int h = 0;
  bool valid() const { return !pixels.empty() && w > 0 && h > 0; }
};
struct SDL_Window;

struct App;
struct Config;
struct ShellState;
// por:
struct TileColor;

class Renderer {
public:
  virtual ~Renderer() = default;

  virtual bool init(SDL_Window *window, const Config &cfg) = 0;

  virtual void beginFrame() = 0;

  virtual void drawShell(ShellState &state, Config &cfg,
                         const ShellActions &actions) = 0;

  virtual void endFrame() = 0;
  virtual void shutdown() = 0;
  // Decode + resize en CPU. Thread-safe (no toca la GPU): lo usa el
  // worker de WallpaperManager.
  virtual bool decodeImage(const std::filesystem::path &path, int cover_w,
                           int cover_h, DecodedImage &out) = 0;
  // Upload a GPU desde píxeles ya decodificados. Solo hilo principal.
  virtual void *createTextureFromPixels(const uint8_t *pixels, int w,
                                        int h) = 0;
  virtual void *loadTextureFromFile(const std::filesystem::path &path,
                                    int *out_w = nullptr, int *out_h = nullptr,
                                    int max_dim = 0,
                                    const TileColor *tint = nullptr,
                                    int cover_w = 0, int cover_h = 0) = 0;

  virtual void freeTexture(void *texture) = 0;
  virtual void getOutputSize(int *w, int *h) = 0;
  virtual void presentBlackFrame() = 0;
};

std::unique_ptr<Renderer> createSdlRenderer();