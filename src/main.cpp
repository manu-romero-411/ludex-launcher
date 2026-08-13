#include <SDL.h>
#include <imgui_impl_sdl2.h> // <-- IMPORTANTE: para alimentar a ImGui

#include <clocale>
#include <iostream>
#include <memory>

#include "app_discovery.h"
#include "assets.h"
#include "backends.h"
#include "config.h"
#include "input_manager.h"
#include "ir_input.h"
#include "launcher.h"
#include "renderer.h"
#include "shell_state.h"
#include "shell_ui.h"

static std::filesystem::path runtimeDir() {
  if (const char *rd = std::getenv("XDG_RUNTIME_DIR"))
    return std::filesystem::path(rd) / "ludex";
  return std::filesystem::path("/tmp") / "ludex";
}

int main(int argc, char **argv) {
  (void)argc;
  (void)argv;

  std::setlocale(LC_ALL, "");

  // Hints DEBEN estar antes de SDL_Init
  SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
  SDL_SetHint(SDL_HINT_GAMECONTROLLER_USE_BUTTON_LABELS, "1");

  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_GAMECONTROLLER) !=
      0) {
    std::cerr << "SDL_Init error: " << SDL_GetError() << std::endl;
    return 1;
  }

  SDL_Window *window =
      SDL_CreateWindow("ludex-launcher", SDL_WINDOWPOS_UNDEFINED,
                       SDL_WINDOWPOS_UNDEFINED, 1920, 1080,
                       SDL_WINDOW_VULKAN | SDL_WINDOW_FULLSCREEN_DESKTOP |
                           SDL_WINDOW_ALLOW_HIGHDPI);

  if (!window) {
    std::cerr << "SDL_CreateWindow error: " << SDL_GetError() << std::endl;
    SDL_Quit();
    return 1;
  }

  Config cfg = loadConfig();
  BackendRegistry backends;
  backends.loadAll();
  InputManager input;
  if (!input.init()) {
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 1;
  }

  auto ir = createDefaultIrInput();
  ir->init();

  ShellState shell;
  shell.refresh(cfg, backends);

  std::unique_ptr<Renderer> renderer = createVulkanRenderer();
  if (!renderer->init(window, cfg)) {
    std::cerr << "No se pudo inicializar el renderer" << std::endl;
    input.shutdown();
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 1;
  }

  int sw = 0, sh = 0;
  renderer->getOutputSize(&sw, &sh);
  loadShellAssets(*renderer, shell, cfg, sw, sh);

  bool running = true;
  bool want_quit = false;

  ShellActions actions;

  actions.launch = [&](const App &app) {
    const Backend *b = backends.find(app.backend);
    if (!b) {
      SDL_Log("[ludex] backend '%s' no disponible", app.backend.c_str());
      return;
    }

    std::filesystem::path ccfg = input.writeControllersConfig(runtimeDir());
    std::vector<std::string> cmd =
        buildBackendCommand(*b, app.run, app.webapp_path, ccfg);
    if (cmd.empty())
      return;

    LaunchHooks hooks;
    hooks.before = [&] {
      input.closeControllers();
      SDL_MinimizeWindow(window);
      SDL_HideWindow(window);
      SDL_FlushEvents(SDL_FIRSTEVENT, SDL_LASTEVENT);
    };
    hooks.after = [&] {
      input.rescanControllers();
      SDL_ShowWindow(window);
      SDL_RaiseWindow(window);
      SDL_FlushEvents(SDL_FIRSTEVENT, SDL_LASTEVENT);
      backends.loadAll(); // recoge backends nuevos
      shell.refresh(cfg, backends);
      loadShellAssets(*renderer, shell, cfg, sw, sh); 
    };
    launchApp(cmd, hooks);
  };

  actions.open_settings = [&] {
    shell.show_settings = true;
    shell.menu_open = false;
    shell.settings_focus = 0;
  };
  actions.quit = [&] { want_quit = true; };
  actions.poweroff = [&] {
    launchApp({"systemctl", "poweroff"}, LaunchHooks{});
  };
  actions.reboot = [&] { launchApp({"systemctl", "reboot"}, LaunchHooks{}); };
  actions.suspend = [&] { launchApp({"systemctl", "suspend"}, LaunchHooks{}); };
  actions.player_status = [&] { return input.playerStatus(); };

  auto handleAction = [&](UiAction a) {
    if (shell.show_settings || shell.show_power) {
      panelInput(shell, cfg, actions, a);
      return;
    }
    switch (a) {
    case UiAction::Left:
      if (shell.menu_open)
        shell.navMenu(-1);
      else if (cfg.side == "top" || cfg.side == "bottom")
        shell.nav(-1);
      break;
    case UiAction::Right:
      if (shell.menu_open)
        shell.navMenu(1);
      else if (cfg.side == "top" || cfg.side == "bottom")
        shell.nav(1);
      break;
    case UiAction::Up:
      if (shell.menu_open)
        shell.navMenu(-1);
      else if (cfg.side == "left" || cfg.side == "right")
        shell.nav(-1);
      break;
    case UiAction::Down:
      if (shell.menu_open)
        shell.navMenu(1);
      else if (cfg.side == "left" || cfg.side == "right")
        shell.nav(1);
      break;
    case UiAction::Select:
      if (shell.menu_open) {
        switch (shell.menu_selected) {
        case 0:
          actions.open_settings();
          break;
        case 1:
          want_quit = true;
          break;
        default:
          shell.show_power = true;
          shell.menu_open = false;
          shell.power_focus = 0;
          break;
        }
      } else if (const App *ap = shell.selectedApp()) {
        actions.launch(*ap);
      }
      break;
    case UiAction::Back:
      if (shell.menu_open)
        shell.menu_open = false;
      break;
    case UiAction::Guide:
      shell.menu_open = !shell.menu_open;
      if (shell.menu_open)
        shell.menu_selected = 0;
      break;
    default:
      break;
    }
  };

  Uint64 last_time = SDL_GetPerformanceCounter();

  while (running) {
    Uint64 now_time = SDL_GetPerformanceCounter();
    float dt = (float)((double)(now_time - last_time) /
                       (double)SDL_GetPerformanceFrequency());
    last_time = now_time;

    SDL_Event event;
    while (SDL_PollEvent(&event)) {
      // >>>>>> IMPRESCINDIBLE: alimentar a ImGui con los eventos <<<<<<
      // Sin esto, el diálogo de configuración y cualquier widget de
      // ImGui es ciego/sordo al ratón, teclado y mando.
      ImGui_ImplSDL2_ProcessEvent(&event);

      if (event.type == SDL_QUIT) {
        running = false;
        continue;
      }
      if (event.type == SDL_WINDOWEVENT &&
          event.window.event == SDL_WINDOWEVENT_CLOSE) {
        running = false;
      }
      // ---- TECLADO ----
      if (event.type == SDL_KEYDOWN) {
        switch (event.key.keysym.sym) {
        case SDLK_UP:
          handleAction(UiAction::Up);
          break;
        case SDLK_DOWN:
          handleAction(UiAction::Down);
          break;
        case SDLK_LEFT:
          handleAction(UiAction::Left);
          break;
        case SDLK_RIGHT:
          handleAction(UiAction::Right);
          break;
        case SDLK_RETURN:
        case SDLK_SPACE:
          handleAction(UiAction::Select);
          break;
        case SDLK_ESCAPE:
          if (shell.show_settings)
            shell.show_settings = false;
          else if (shell.menu_open)
            shell.menu_open = false;
          break;
        case SDLK_F1:
        case SDLK_HOME:
          handleAction(UiAction::Guide);
          break;
        case SDLK_F5:
          backends.loadAll();
          shell.refresh(cfg, backends);
          loadShellAssets(*renderer, shell, cfg, sw, sh);
          break;
        case SDLK_w:
          loadAllWallpapers(*renderer, shell, cfg, sw, sh);
          break;
        default:
          break;
        }
      }

      input.handleEvent(event);
    }

    // ---- MANDO (UiInput desde InputManager) ----
    UiInput ui;
    input.update();

    while (input.poll(ui)) {
      if (ui.player != cfg.active_player)
        continue;
      handleAction(ui.action);
    }

    if (!shell.show_settings && !shell.show_power) {
      shell.update(dt, cfg);
    }

    if (want_quit)
      running = false;

    // Solo animamos el carrusel si no hay diálogo abierto
    // (ImGui ya gestiona su propia navegación por mando/teclado).
    if (!shell.show_settings) {
      shell.update(dt, cfg);
    }

    renderer->beginFrame();
    renderer->drawShell(shell, cfg, actions);
    renderer->endFrame();
  }

  freeShellAssets(*renderer, shell);
  renderer->shutdown();
  ir->shutdown();
  input.shutdown();
  SDL_DestroyWindow(window);
  SDL_Quit();
  return 0;
}