
#include <SDL.h>
#include <SDL_image.h>
#include <imgui_impl_sdl2.h> // <-- IMPORTANTE: para alimentar a ImGui

#include <clocale>
#include <iostream>
#include <memory>

#include "app_discovery.h"
#include "assets.h"
#include "audio_manager.h"
#include "backends.h"
#include "config.h"
#include "input_manager.h"
#include "ir_input.h"
#include "launcher.h"
#include "renderer.h"
#include "shell_state.h"
#include "shell_ui.h"
#include "util.h"

static std::filesystem::path runtimeDir() {
  if (const char *rd = std::getenv("XDG_RUNTIME_DIR"))
    return std::filesystem::path(rd) / "ludex";
  return std::filesystem::path("/tmp") / "ludex";
}

static SDL_Surface *loadWindowIcon(const std::filesystem::path &icons_dir) {
  const char *names[] = {"ludex.svg", "ludex.png", "app.svg", "app.png"};
  for (const char *n : names) {
    std::filesystem::path p = icons_dir / n;
    std::error_code ec;
    if (!std::filesystem::exists(p, ec))
      continue;
    SDL_Surface *s = nullptr;
    if (p.extension() == ".svg") {
      SDL_RWops *rw = SDL_RWFromFile(p.c_str(), "rb");
      if (rw) {
        s = IMG_LoadSizedSVG_RW(rw, 256, 256);
        SDL_RWclose(rw);
      }
    } else {
      s = IMG_Load(p.c_str());
    }
    if (s)
      return s;
  }
  return nullptr;
}

int main(int argc, char **argv) {
  (void)argc;
  (void)argv;

  std::setlocale(LC_ALL, "");

  SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
  SDL_SetHint(SDL_HINT_GAMECONTROLLER_USE_BUTTON_LABELS, "1");
  SDL_SetHint(SDL_HINT_APP_NAME, "ludex"); // app_id Wayland

  const Uint32 sdl_flags =
      SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_GAMECONTROLLER;

  if (!std::getenv("SDL_VIDEODRIVER"))
    setenv("SDL_VIDEODRIVER", "wayland", 0);

  if (SDL_Init(sdl_flags) != 0) {
    SDL_Log("[ludex] '%s' falló (%s); reintentando con x11",
            std::getenv("SDL_VIDEODRIVER"), SDL_GetError());
    setenv("SDL_VIDEODRIVER", "x11", 1);
    if (SDL_Init(sdl_flags) != 0) {
      std::cerr << "SDL_Init error: " << SDL_GetError() << std::endl;
      return 1;
    }
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

  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG | IMG_INIT_WEBP);
  if (SDL_Surface *icon = loadWindowIcon(cfg.icons_dir)) {
    SDL_SetWindowIcon(window, icon);
    SDL_FreeSurface(icon);
  }

  // En main(), después de crear window y cargar config:
  AudioManager audio;
  if (!audio.init()) {
    std::cerr << "Failed to initialize audio\n";
  }

  // Cargar música desde múltiples ubicaciones (en orden de prioridad):
  std::vector<std::filesystem::path> music_dirs = {
      cfg.music_dir, // del INI
      util::exeDir() / "music",
      std::filesystem::path(std::getenv("HOME") ? std::getenv("HOME") : "") /
          ".config" / "ludex" / "music",
      std::filesystem::path("/usr/share/ludex/music")};

  for (const auto &dir : music_dirs) {
    if (std::filesystem::exists(dir)) {
      audio.loadMusicFromDirectory(dir);
      break; // usar la primera que exista
    }
  }

  // Cargar efectos de sonido:
  std::filesystem::path sounds_dir = util::exeDir() / "resources" / "sounds";
  audio.loadSoundEffects(sounds_dir);

  // Iniciar música de fondo:
  audio.startMusic();

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
  DragState drag;
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
  loadUiIcons(*renderer, shell, cfg, sh);
  bool running = true;
  bool want_quit = false;
  bool app_running = false;
  ShellActions actions;

  actions.launch = [&](const App &app) {
    const Backend *b = backends.find(app.backend);
    if (!b) {
      SDL_Log("[ludex] backend '%s' no disponible", app.backend.c_str());
      return;
    }

    std::string ccfg_str = input.controllersConfigString();
    std::filesystem::path ccfg_file =
        input.writeControllersConfig(runtimeDir());

    std::vector<std::string> cmd =
        buildBackendCommand(*b, app.run, app.webapp_path, ccfg_str, ccfg_file);
    if (cmd.empty())
      return;

    LaunchHooks hooks;
    hooks.before = [&] {
      audio.stopMusic();
      app_running = true;
      input.closeControllers();
      renderer->presentBlackFrame();
      SDL_FlushEvents(SDL_FIRSTEVENT, SDL_LASTEVENT);
    };
    hooks.after = [&] {
      drag.reset();
      input.clearTransientState();
      renderer->getOutputSize(&sw, &sh);
      input.rescanControllers();
      app_running = false;
      SDL_RaiseWindow(window);
      SDL_FlushEvents(SDL_FIRSTEVENT, SDL_LASTEVENT);
      backends.loadAll();
      shell.refreshAndFreeOldAssets(*renderer, cfg, backends);
      loadShellAssets(*renderer, shell, cfg, sw, sh);
      audio.startMusic();
      shell.dragging = false;
      shell.has_momentum = false;
      shell.drag_velocity = 0.0f;
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
  actions.reload_ui_icons = [&] { loadUiIcons(*renderer, shell, cfg, sh); };
  auto handleAction = [&](UiAction a) {
    if (shell.show_controllers) {
      controllersInput(shell, cfg, actions, a);
      return;
    }

    if (shell.show_settings || shell.show_power) {
      panelInput(shell, cfg, actions, a);
      return;
    }
    switch (a) {
    case UiAction::Left:
      if (shell.menu_open)
        shell.navMenu(-1);
      else if (cfg.side == "top" || cfg.side == "bottom") {
        shell.nav(-1);
        audio.playScrollSound(); // sonido al navegar
      }

      break;
    case UiAction::Right:
      if (shell.menu_open)
        shell.navMenu(1);
      else if (cfg.side == "top" || cfg.side == "bottom") {
        shell.nav(1);
        audio.playScrollSound(); // sonido al navegar
      }

      break;
    case UiAction::Up:
      if (shell.menu_open)
        shell.navMenu(-1);
      else if (cfg.side == "left" || cfg.side == "right") {
        shell.nav(-1);
        audio.playScrollSound(); // sonido al navegar
      }
      break;
    case UiAction::Down:
      if (shell.menu_open)
        shell.navMenu(1);
      else if (cfg.side == "left" || cfg.side == "right") {
        shell.nav(1);
        audio.playScrollSound(); // sonido al navegar
      }
      break;
    case UiAction::Select:
      audio.playSelectSound(); // sonido al seleccionar
      if (shell.menu_open) {
        switch (shell.menu_selected) {
        case 0: // CONFIGURACIÓN
          actions.open_settings();
          break;
        case 1: // CONTROLLERS
          if (actions.open_controllers)
            actions.open_controllers();
          break;
        case 2: // SALIR
          want_quit = true;
          break;
        case 3: // SHUTDOWN
        default:
          shell.show_power = true;
          shell.menu_open = false;
          shell.power_focus = 0;
          break;
        }
      } else if (shell.selectedApp()) {
        shell.pending_launch = shell.selected;
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
  auto guidsFromCfg = [](const Config &c) {
    std::vector<std::string> v;
    for (int i = 0; i < Config::MAX_PLAYERS; ++i)
      v.push_back(c.controller_guid[i]);
    return v;
  };

  input.applyAssignment(guidsFromCfg(cfg)); // tras input.init()

  actions.open_controllers = [&] {
    shell.show_controllers = true;
    shell.menu_open = false;
    shell.controllers_focus = 0;
    shell.controller_pick_player = -1;
  };
  actions.apply_controllers = [&] { input.applyAssignment(guidsFromCfg(cfg)); };
  actions.devices = [&] { return input.devices(); };
  Uint64 last_time = SDL_GetPerformanceCounter();

  while (running) {
    Uint64 now_time = SDL_GetPerformanceCounter();
    float dt = (float)((double)(now_time - last_time) /
                       (double)SDL_GetPerformanceFrequency());
    last_time = now_time;

    SDL_Event event;
    while (SDL_PollEvent(&event)) {
      // SDL_QUIT siempre se respeta (cerrar ventana del SO)
      if (event.type == SDL_QUIT) {
        running = false;
        continue;
      }
      if (event.type == SDL_WINDOWEVENT &&
          event.window.event == SDL_WINDOWEVENT_CLOSE) {
        running = false;
        continue;
      }

      // Mientras la app hija corre: drenar y descartar TODO
      if (app_running)
        continue;

      ImGui_ImplSDL2_ProcessEvent(&event);

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
          handleAction(UiAction::Back);
          break;
        case SDLK_F1:
        case SDLK_HOME:
          handleAction(UiAction::Guide);
          break;
        case SDLK_F5:
          backends.loadAll();
          shell.refreshAndFreeOldAssets(*renderer, cfg, backends);
          // Solo recargar iconos de apps, NO wallpapers
          {
            int icon_max = (int)(sh * cfg.icon_sel_pct * 2.0f);
            for (auto &app : shell.apps) {
              if (!app.icon_path.empty() && !app.icon_texture) {
                app.icon_texture = renderer->loadTextureFromFile(
                    app.icon_path, nullptr, nullptr, icon_max,
                    app.has_icon_tint ? &app.icon_tint : nullptr);
              }
            }
          }
          break;
        case SDLK_w:
          shell.nextWallpaper();
          break;
        case SDLK_F6:
          loadAllWallpapers(*renderer, shell, cfg, sw, sh); // relee del disco
          break;
        default:
          break;
        }
      }
      // ---- MOUSE WHEEL (scroll en tiles) ----
      if (event.type == SDL_MOUSEWHEEL) {
        if (!shell.menu_open && !shell.show_settings && !shell.show_power &&
            !shell.show_controllers) {
          if (event.wheel.y != 0) {
            int dy = (event.wheel.y > 0) ? -1 : 1;
            shell.nav(dy);
            audio.playScrollSound();
          }
        }
      }

      // ---- Estado estático para drag con threshold ----
      // Necesario porque el evento MOUSEMOTION no lleva el botón de origen.
      static bool mouse_down = false;
      static float mouse_down_x = 0.0f, mouse_down_y = 0.0f;
      static bool drag_active = false; // true solo tras superar threshold
      static constexpr float DRAG_THRESHOLD =
          12.0f; // píxeles antes de considerar drag

      static float finger_down_x = 0.0f, finger_down_y = 0.0f;
      static bool finger_down = false;
      static bool touch_drag_active = false;

      auto axisPos = [&](float px, float py) -> float {
        return (cfg.side == "left" || cfg.side == "right") ? py : px;
      };
      auto startDragState = [&](float px, float py) {
        float pos = axisPos(px, py);
        shell.drag_start_pos = pos;
        shell.drag_start_offset = shell.offset;
        shell.drag_last_pos = pos;
        shell.drag_last_time = SDL_GetTicks();
        shell.drag_velocity = 0.0f;
        shell.has_momentum = false;
      };
      auto doDrag = [&](float px, float py) {
        float pos = axisPos(px, py);
        float main_axis =
            (cfg.side == "top" || cfg.side == "bottom") ? (float)sw : (float)sh;
        float k = cfg.tile_sel_ratio;
        float slot_size = main_axis / ((float)(cfg.visible_items - 1) + k);

        float delta_px = pos - shell.drag_start_pos;
        shell.offset = shell.drag_start_offset - delta_px / slot_size;

        Uint32 now = SDL_GetTicks();
        float dt_ms = (float)(now - shell.drag_last_time);
        if (dt_ms > 1.0f) {
          float px_delta = pos - shell.drag_last_pos;
          // velocidad en unidades/segundo
          shell.drag_velocity = -(px_delta / dt_ms) * 1000.0f / slot_size;
        }
        shell.drag_last_pos = pos;
        shell.drag_last_time = now;
      };
      auto endDrag = [&]() {
        shell.dragging = false;
        shell.has_momentum = (std::fabs(shell.drag_velocity) > 2.0f);
        if (!shell.has_momentum)
          shell.drag_velocity = 0.0f;
      };

      // ---- MOUSE DRAG con threshold ----
      if (event.type == SDL_MOUSEBUTTONDOWN &&
          event.button.button == SDL_BUTTON_LEFT) {
        if (!shell.menu_open && !shell.show_settings && !shell.show_power &&
            !shell.show_controllers) {
          mouse_down = true;
          drag_active = false;
          mouse_down_x = (float)event.button.x;
          mouse_down_y = (float)event.button.y;
          startDragState(mouse_down_x, mouse_down_y);
        }
      }
      if (event.type == SDL_MOUSEBUTTONUP &&
          event.button.button == SDL_BUTTON_LEFT) {
        if (drag_active) {
          endDrag();
        }
        mouse_down = false;
        drag_active = false;
      }
      if (event.type == SDL_MOUSEMOTION) {
        if (mouse_down && !drag_active) {
          float dx = (float)event.motion.x - mouse_down_x;
          float dy = (float)event.motion.y - mouse_down_y;
          if (std::sqrt(dx * dx + dy * dy) > DRAG_THRESHOLD) {
            drag_active = true;
            shell.dragging = true;
          }
        }
        if (drag_active) {
          doDrag((float)event.motion.x, (float)event.motion.y);
        }
      }

      // ---- TOUCH DRAG con threshold ----
      if (event.type == SDL_FINGERDOWN) {
        if (!shell.menu_open && !shell.show_settings && !shell.show_power &&
            !shell.show_controllers) {
          finger_down = true;
          touch_drag_active = false;
          finger_down_x = event.tfinger.x * (float)sw;
          finger_down_y = event.tfinger.y * (float)sh;
          startDragState(finger_down_x, finger_down_y);
        }
      }
      if (event.type == SDL_FINGERUP) {
        if (touch_drag_active) {
          endDrag();
        }
        finger_down = false;
        touch_drag_active = false;
      }
      if (event.type == SDL_FINGERMOTION) {
        float fx = event.tfinger.x * (float)sw;
        float fy = event.tfinger.y * (float)sh;
        if (finger_down && !touch_drag_active) {
          float dx = fx - finger_down_x;
          float dy = fy - finger_down_y;
          if (std::sqrt(dx * dx + dy * dy) > DRAG_THRESHOLD) {
            touch_drag_active = true;
            shell.dragging = true;
          }
        }
        if (touch_drag_active) {
          doDrag(fx, fy);
        }
      }
      input.handleEvent(event);
    }

    // ---- MANDO (UiInput desde InputManager) ----

    // ---- MANDO (UiInput desde InputManager) ----
    if (!app_running) {
      input.update();
      UiInput ui;
      while (input.poll(ui)) {
        if (!cfg.all_players_ui && ui.player != cfg.active_player)
          continue;
        handleAction(ui.action);
      }
    }

    if (!shell.show_settings && !shell.show_power && !shell.show_controllers) {
      shell.update(dt, cfg);
    }

    if (want_quit)
      running = false;

    renderer->beginFrame();
    renderer->drawShell(shell, cfg, actions);
    renderer->endFrame();
  }
  audio.shutdown();
  freeShellAssets(*renderer, shell);
  renderer->shutdown();
  ir->shutdown();
  input.shutdown();
  SDL_DestroyWindow(window);
  SDL_Quit();
  return 0;
}