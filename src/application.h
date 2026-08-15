#include "audio/audio_manager.h"
#include "core/app_backends.h"
#include "core/bluetooth_manager.h"
#include "core/config.h"
#include "input/input_manager.h"
#include "input/ir_input.h"
#include "ui/render/renderer.h"
#include "ui/shell/shell_state.h"
#include <SDL.h>
#include <filesystem>
class Application {
public:
  bool init();
  int run();
  void shutdown();

private:
  void setupActions();
  void startAppFade(bool fade_in);
  void updateAppFade(float dt);
  void renderAppFade();
  void processEvents(float dt);
  void handleAction(UiAction a);
  void handleKeyboard(const SDL_Event &e);
  void handleMouseDrag(const SDL_Event &e);
  void handleTouchDrag(const SDL_Event &e);
  void handleBtEvent(const BtEvent &ev);
  
  Uint64 bt_refresh_last_ = 0;
  SDL_Surface *loadWindowIcon();
  std::filesystem::path runtimeDir();

  // Servicios
  SDL_Window *window_ = nullptr;
  Config cfg_;

  // IMPORTANTE: renderer_ ANTES de shell_ para que
  // shell_ se destruya PRIMERO (orden inverso)
  std::unique_ptr<Renderer> renderer_;

  ShellState shell_;
  BackendRegistry backends_;
  InputManager input_;
  AudioManager audio_;
  std::unique_ptr<IrInput> ir_;
  DragState drag_;
  ShellActions actions_;
  BluetoothManager bluetooth_;
  // Fade de transición de apps
  bool app_fade_active_ = false;
  bool app_fade_in_ = true; // true = fade a negro, false = fade desde negro
  float app_fade_progress_ = 0.0f;
  float app_fade_duration_ = 0.3f;
  int pending_fade_launch_ = -1;

  // bt
  bool bt_rescan_pending_ = false;
  bool running_ = false;
  bool want_quit_ = false;
  bool app_running_ = false;
  int sw_ = 0, sh_ = 0;
  Uint64 last_time_ = 0;
  Uint64 bt_rescan_time_ = 0;
};