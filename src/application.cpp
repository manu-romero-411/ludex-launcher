#include "application.h"
#include "core/i18n.h"
#include "core/launcher.h"
#include "ui/assets.h"
#include "ui/ui_input_handlers.h"
#include "util.h"
#include <SDL_image.h>
#include <cmath>
#include <imgui_impl_sdl2.h>
#include <iostream>

namespace {

struct DragEventInfo {
  bool is_down = false;
  bool is_up = false;
  bool is_motion = false;
  float x = 0.0f;
  float y = 0.0f;
};

DragEventInfo classifyDragEvent(const SDL_Event &e, int sw, int sh) {
  DragEventInfo info;
  switch (e.type) {
  case SDL_MOUSEBUTTONDOWN:
    if (e.button.button == SDL_BUTTON_LEFT) {
      info.is_down = true;
      info.x = (float)e.button.x;
      info.y = (float)e.button.y;
    }
    break;
  case SDL_MOUSEBUTTONUP:
    if (e.button.button == SDL_BUTTON_LEFT) {
      info.is_up = true;
      info.x = (float)e.button.x;
      info.y = (float)e.button.y;
    }
    break;
  case SDL_MOUSEMOTION:
    info.is_motion = true;
    info.x = (float)e.motion.x;
    info.y = (float)e.motion.y;
    break;
  case SDL_FINGERDOWN:
    info.is_down = true;
    info.x = e.tfinger.x * (float)sw;
    info.y = e.tfinger.y * (float)sh;
    break;
  case SDL_FINGERUP:
    info.is_up = true;
    info.x = e.tfinger.x * (float)sw;
    info.y = e.tfinger.y * (float)sh;
    break;
  case SDL_FINGERMOTION:
    info.is_motion = true;
    info.x = e.tfinger.x * (float)sw;
    info.y = e.tfinger.y * (float)sh;
    break;
  default:
    break;
  }
  return info;
}

} // namespace

std::filesystem::path Application::runtimeDir() {
  if (const char *rd = std::getenv("XDG_RUNTIME_DIR"))
    return std::filesystem::path(rd) / "ludex";
  return std::filesystem::path("/tmp") / "ludex";
}

SDL_Surface *Application::loadWindowIcon() {
  const char *names[] = {"ludex.svg", "ludex.png", "app.svg", "app.png"};
  for (const char *n : names) {
    std::filesystem::path p = cfg_.icons_dir / n;
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

void Application::setupActions() {
  actions_.launch = [this](const App &app) {
    const Backend *b = backends_.find(app.backend);
    if (!b) {
      SDL_Log("[ludex] backend '%s' no disponible", app.backend.c_str());
      return;
    }

    std::string ccfg_str = input_.controllersConfigString();
    std::filesystem::path ccfg_file =
        input_.writeControllersConfig(runtimeDir());

    std::vector<std::string> cmd =
        buildBackendCommand(*b, app.run, app.webapp_path, ccfg_str, ccfg_file);
    if (cmd.empty())
      return;

    LaunchHooks hooks;
    hooks.before = [this] {
      audio_.stopMusic();
      app_running_ = true;
      input_.closeControllers();
      renderer_->presentBlackFrame();
      SDL_FlushEvents(SDL_FIRSTEVENT, SDL_LASTEVENT);
    };
    hooks.after = [this] {
      startAppFade(false);
      drag_.reset();
      input_.clearTransientState();
      renderer_->getOutputSize(&sw_, &sh_);
      input_.rescanControllers();
      app_running_ = false;
      SDL_RaiseWindow(window_);

      // Ignorar inputs residuales durante 100ms sin bloquear el hilo
      input_ignore_until_ = SDL_GetTicks() + 100;

      backends_.loadAll();
      shell_.refresh(cfg_, backends_);
      loadShellAssets(*renderer_, shell_, cfg_, sw_, sh_);
      audio_.startMusic();
      shell_.dragging = false;
      shell_.has_momentum = false;
      shell_.drag_velocity = 0.0f;
    };
    launchApp(cmd, hooks);
  };

  actions_.open_settings = [this] {
    shell_.show_settings = true;
    shell_.settings_focus = 0;
  };
  actions_.open_about = [this] {
    shell_.show_about = true;
    shell_.about_focus = 0;
  };
  actions_.quit = [this] { want_quit_ = true; };
  actions_.poweroff = [] {
    launchApp({"systemctl", "poweroff"}, LaunchHooks{});
  };
  actions_.reboot = [] { launchApp({"systemctl", "reboot"}, LaunchHooks{}); };
  actions_.suspend = [] { launchApp({"systemctl", "suspend"}, LaunchHooks{}); };
  actions_.player_status = [this] { return input_.playerStatus(); };
  actions_.reload_ui_icons = [this] {
    loadUiIcons(*renderer_, shell_, cfg_, sh_);
  };

  actions_.open_controllers = [this] {
    shell_.show_controllers = true;
    shell_.controllers_focus = 0;
    shell_.controller_pick_player = -1;
  };
  actions_.apply_controllers = [this] {
    std::vector<std::string> guids;
    for (int i = 0; i < Config::MAX_PLAYERS; ++i)
      guids.push_back(cfg_.controller_guid[i]);
    input_.applyAssignment(guids);
  };
  actions_.devices = [this] { return input_.devices(); };

  actions_.open_bluetooth = [this] {
    shell_.show_bluetooth = true;
    shell_.bluetooth_focus = 0;
  };
  actions_.bluetooth_available = [this] { return bluetooth_.available(); };
  actions_.bluetooth_scan = [this] { bluetooth_.requestScan(12); };
  actions_.bluetooth_connect = [this](const std::string &m) {
    bluetooth_.requestConnect(m);
  };
  actions_.bluetooth_disconnect = [this](const std::string &m) {
    bluetooth_.requestDisconnect(m);
  };
  actions_.bluetooth_remove = [this](const std::string &m) {
    bluetooth_.requestRemove(m);
  };
  actions_.bluetooth_pair = [this](const std::string &m) {
    bluetooth_.requestPair(m);
  };
  actions_.bluetooth_devices = [this] { return bluetooth_.devices(); };
  actions_.bluetooth_discovered = [this] {
    return bluetooth_.discoveredDevices();
  };
  actions_.bluetooth_scanning = [this] { return bluetooth_.isScanning(); };
  actions_.bluetooth_scan_remaining = [this] {
    return bluetooth_.scanRemainingSec();
  };
  actions_.bluetooth_submit_pin = [this](const std::string &m,
                                         const std::string &p) {
    shell_.toasts.push(shell_.ui_icons.bluetooth.get(), _("BLUETOOTH"),
                       _("PAIRING..."));
    bluetooth_.submitPin(m, p);
  };
  actions_.bluetooth_cancel_pin = [this] { bluetooth_.cancelPin(); };
  actions_.bluetooth_cancel_scan = [this] { bluetooth_.requestCancelScan(); };
}

bool Application::init() {
  SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
  SDL_SetHint(SDL_HINT_GAMECONTROLLER_USE_BUTTON_LABELS, "1");
  SDL_SetHint(SDL_HINT_APP_NAME, "ludex");

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
      return false;
    }
  }

  window_ = SDL_CreateWindow("ludex-launcher", SDL_WINDOWPOS_UNDEFINED,
                             SDL_WINDOWPOS_UNDEFINED, 1920, 1080,
                             SDL_WINDOW_OPENGL | SDL_WINDOW_FULLSCREEN_DESKTOP |
                                 SDL_WINDOW_ALLOW_HIGHDPI);

  // Forzar el driver OpenGL (evita que SDL intente Direct3D/Metal/etc.)
  SDL_SetHint(SDL_HINT_RENDER_DRIVER, "opengl");

  // descomentar si funciona mal en sandy bridge
  // SDL_SetHint(SDL_HINT_VIDEO_X11_NET_WM_BYPASS_COMPOSITOR, "1");

  if (!window_) {
    std::cerr << "SDL_CreateWindow error: " << SDL_GetError() << std::endl;
    SDL_Quit();
    return false;
  }

  cfg_ = loadConfig();
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG | IMG_INIT_WEBP);

  if (SDL_Surface *icon = loadWindowIcon()) {
    SDL_SetWindowIcon(window_, icon);
    SDL_FreeSurface(icon);
  }

  if (!audio_.init()) {
    std::cerr << "Failed to initialize audio\n";
  }

  std::vector<std::filesystem::path> music_dirs = {
      cfg_.music_dir, util::exeDir() / "music",
      std::filesystem::path(std::getenv("HOME") ? std::getenv("HOME") : "") /
          ".config" / "ludex" / "music",
      std::filesystem::path("/usr/share/ludex/music")};
  for (const auto &dir : music_dirs) {
    if (std::filesystem::exists(dir)) {
      audio_.loadMusicFromDirectory(dir);
      break;
    }
  }

  std::filesystem::path sounds_dir = util::exeDir() / "resources" / "sounds";
  audio_.loadSoundEffects(sounds_dir);
  audio_.startMusic();

  backends_.loadAll();

  if (bluetooth_.available()) {
    bluetooth_.requestRefresh();
    SDL_Log("[ludex] Bluetooth available");
  } else {
    SDL_Log("[ludex] Bluetooth not available");
  }

  if (!input_.init()) {
    SDL_DestroyWindow(window_);
    SDL_Quit();
    return false;
  }

  ir_ = createDefaultIrInput();
  ir_->init();

  shell_.refresh(cfg_, backends_);

  renderer_ = createSdlRenderer();
  if (!renderer_->init(window_, cfg_)) {
    std::cerr << "No se pudo inicializar el renderer" << std::endl;
    input_.shutdown();
    SDL_DestroyWindow(window_);
    SDL_Quit();
    return false;
  }

  renderer_->getOutputSize(&sw_, &sh_);
  loadShellAssets(*renderer_, shell_, cfg_, sw_, sh_);
  loadUiIcons(*renderer_, shell_, cfg_, sh_);

  setupActions();

  std::vector<std::string> guids;
  for (int i = 0; i < Config::MAX_PLAYERS; ++i)
    guids.push_back(cfg_.controller_guid[i]);
  input_.applyAssignment(guids);

  running_ = true;
  last_time_ = SDL_GetPerformanceCounter();
  return true;
}

int Application::run() {
  while (running_) {
    Uint64 now_time = SDL_GetPerformanceCounter();
    float dt = (float)((double)(now_time - last_time_) /
                       (double)SDL_GetPerformanceFrequency());
    last_time_ = now_time;

    updateAppFade(dt);
    shell_.toasts.update(dt); // NUEVO: envejecer toasts
    processEvents(dt);

    if (!app_running_ && !app_fade_active_) {
      input_.update();
      UiInput ui;
      while (input_.poll(ui)) {
        if (!cfg_.all_players_ui && ui.player != cfg_.active_player)
          continue;
        handleAction(ui.action);
      }
    }

    if (!shell_.show_settings && !shell_.show_power &&
        !shell_.show_controllers && !shell_.show_bluetooth &&
        !shell_.show_bluetooth_scan) {
      shell_.update(dt, cfg_, *renderer_, sw_, sh_);
    }

    audio_.update();
    bluetooth_.setAutoRefresh(shell_.show_bluetooth);
    {
      BtEvent ev;
      while (bluetooth_.pollEvent(ev))
        handleBtEvent(ev);
    }

    // Rescan diferido tras conectar/desconectar BT
    if (bt_rescan_pending_) {
      Uint64 now = SDL_GetPerformanceCounter();
      double elapsed = (double)(now - bt_rescan_time_) /
                       (double)SDL_GetPerformanceFrequency();
      if (elapsed > 1.0) {
        input_.rescanControllers();
        shell_.invalidatePanels(); // NUEVO
        bt_rescan_pending_ = false;
        SDL_Log("[ludex] InputManager rescan tras BT connect/disconnect");
      }
    }
    if (want_quit_)
      running_ = false;

    if (!app_running_ && shell_.pending_launch >= 0) {
      int idx = shell_.pending_launch;
      shell_.pending_launch = -1;
      pending_fade_launch_ = idx;
      startAppFade(true);
    }

    if (!app_running_ && pending_fade_launch_ >= 0 && app_fade_in_ &&
        app_fade_progress_ >= 1.0f) {
      int idx = pending_fade_launch_;
      pending_fade_launch_ = -1;
      if (idx >= 0 && idx < (int)shell_.apps.size()) {
        actions_.launch(shell_.apps[idx]);
        last_time_ = SDL_GetPerformanceCounter();
      }
    }
    shell_.updatePanelAnimation(dt);

    renderer_->beginFrame();
    shell_.icon_cache.beginFrame();
    renderer_->drawShell(shell_, cfg_, actions_);
    renderAppFade();
    renderer_->endFrame();
    shell_.icon_cache.evict(8);
  }
  return 0;
}

void Application::shutdown() {
  audio_.shutdown();
  freeShellAssets(*renderer_, shell_);
  if (renderer_)
    renderer_->shutdown();
  if (ir_)
    ir_->shutdown();
  input_.shutdown();
  if (window_)
    SDL_DestroyWindow(window_);
  SDL_Quit();
}

void Application::processEvents(float dt) {
  SDL_Event event;
  Uint32 now = SDL_GetTicks();
  bool ignoring_inputs = (now < input_ignore_until_);

  while (SDL_PollEvent(&event)) {
    // Siempre procesar eventos críticos del sistema
    if (event.type == SDL_QUIT ||
        (event.type == SDL_WINDOWEVENT &&
         event.window.event == SDL_WINDOWEVENT_CLOSE)) {
      running_ = false;
      continue;
    }

    // Bloquear completamente cuando una app está corriendo o hay fade activo
    if (app_running_ || app_fade_active_) {
      continue;
    }

    // Si estamos en período de ignorancia, descartar eventos de input
    // pero seguir procesando eventos del sistema (device add/remove, etc.)
    if (ignoring_inputs) {
      // Permitir eventos de dispositivos (para que el rescan funcione)
      if (event.type == SDL_CONTROLLERDEVICEADDED ||
          event.type == SDL_CONTROLLERDEVICEREMOVED ||
          event.type == SDL_JOYDEVICEADDED ||
          event.type == SDL_JOYDEVICEREMOVED) {
        input_.handleEvent(event);
      }
      continue;
    }

    ImGui_ImplSDL2_ProcessEvent(&event);

    if (event.type == SDL_KEYDOWN) {
      handleKeyboard(event);
    } else if (event.type == SDL_MOUSEWHEEL) {
      if (!shell_.anyPanelOpen()) {
        if (event.wheel.y != 0) {
          int dy = (event.wheel.y > 0) ? -1 : 1;
          shell_.nav(dy);
          audio_.playScrollSound();
        }
      }
    }

    handleDragEvent(event);

    // Si cambia la lista de mandos, invalidar specs cacheados
    if (event.type == SDL_CONTROLLERDEVICEADDED ||
        event.type == SDL_CONTROLLERDEVICEREMOVED ||
        event.type == SDL_JOYDEVICEADDED ||
        event.type == SDL_JOYDEVICEREMOVED) {
      shell_.invalidatePanels();
    }

    input_.handleEvent(event);
  }
}
void Application::handleKeyboard(const SDL_Event &e) {
  switch (e.key.keysym.sym) {
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
    backends_.loadAll();
    shell_.refresh(cfg_, backends_);
    break;
  case SDLK_x:
    handleAction(UiAction::Alt);
    break;
  case SDLK_w:
    shell_.nextWallpaper(*renderer_, sw_, sh_);
    break;
  case SDLK_F6:
    shell_.wallpapers.discover(cfg_);
    shell_.wallpapers.loadInitial(*renderer_, sw_, sh_);
    break;
  default:
    break;
  }
}

void Application::handleDragEvent(const SDL_Event &e) {
  static constexpr float DRAG_THRESHOLD = 12.0f;

  DragEventInfo info = classifyDragEvent(e, sw_, sh_);
  if (!info.is_down && !info.is_up && !info.is_motion)
    return;

  auto axisPos = [this](float px, float py) -> float {
    return isHorizontal(cfg_.side) ? px : py;
  };
  auto startDragState = [this, &axisPos](float px, float py) {
    float pos = axisPos(px, py);
    shell_.drag_start_pos = pos;
    shell_.drag_start_offset = shell_.offset;
    shell_.drag_last_pos = pos;
    shell_.drag_last_time = SDL_GetTicks();
    shell_.drag_velocity = 0.0f;
    shell_.has_momentum = false;
  };
  auto doDrag = [this, &axisPos](float px, float py) {
    float pos = axisPos(px, py);
    float main_axis = isHorizontal(cfg_.side) ? (float)sw_ : (float)sh_;
    float slot_size =
        main_axis / ((float)(cfg_.visible_items - 1) + cfg_.tile_sel_ratio);
    float delta_px = pos - shell_.drag_start_pos;
    shell_.offset = shell_.drag_start_offset - delta_px / slot_size;
    Uint32 now = SDL_GetTicks();
    float dt_ms = (float)(now - shell_.drag_last_time);
    if (dt_ms > 1.0f) {
      float px_delta = pos - shell_.drag_last_pos;
      shell_.drag_velocity = -(px_delta / dt_ms) * 1000.0f / slot_size;
    }
    shell_.drag_last_pos = pos;
    shell_.drag_last_time = now;
  };
  auto endDrag = [this]() {
    shell_.dragging = false;
    shell_.has_momentum = (std::fabs(shell_.drag_velocity) > 2.0f);
    if (!shell_.has_momentum)
      shell_.drag_velocity = 0.0f;
  };

  if (info.is_down) {
    if (!shell_.anyPanelOpen()) {
      drag_.down = true;
      drag_.drag_active = false;
      drag_.down_x = info.x;
      drag_.down_y = info.y;
      startDragState(info.x, info.y);
    }
  }
  if (info.is_up) {
    if (drag_.drag_active)
      endDrag();
    drag_.down = false;
    drag_.drag_active = false;
  }
  if (info.is_motion) {
    if (drag_.down && !drag_.drag_active) {
      float dx = info.x - drag_.down_x;
      float dy = info.y - drag_.down_y;
      if (std::sqrt(dx * dx + dy * dy) > DRAG_THRESHOLD) {
        drag_.drag_active = true;
        shell_.dragging = true;
      }
    }
    if (drag_.drag_active) {
      doDrag(info.x, info.y);
    }
  }
}

void Application::handleAction(UiAction a) {
  if (shell_.show_controllers) {
    controllersInput(shell_, cfg_, actions_, a);
    return;
  }
  if (shell_.anyPanelOpen()) {
    panelInput(shell_, cfg_, actions_, a);
    return;
  }

  switch (a) {
  case UiAction::Left:
    if (isHorizontal(cfg_.side)) {
      shell_.nav(-1);
      audio_.playScrollSound();
    }
    break;
  case UiAction::Right:
    if (isHorizontal(cfg_.side)) {
      shell_.nav(1);
      audio_.playScrollSound();
    }
    break;
  case UiAction::Up:
    if (!isHorizontal(cfg_.side)) {
      shell_.nav(-1);
      audio_.playScrollSound();
    }
    break;
  case UiAction::Down:
    if (!isHorizontal(cfg_.side)) {
      shell_.nav(1);
      audio_.playScrollSound();
    }
    break;
  case UiAction::Select:
    audio_.playSelectSound();
    if (shell_.selectedApp())
      shell_.pending_launch = shell_.selected;
    break;
  case UiAction::Guide:
    shell_.show_system = true;
    shell_.system_focus = 0;
    break;
  default:
    break;
  }
}

void Application::startAppFade(bool fade_in) {
  app_fade_active_ = true;
  app_fade_in_ = fade_in;
  app_fade_progress_ = fade_in ? 0.0f : 1.0f;
}

void Application::updateAppFade(float dt) {
  if (!app_fade_active_)
    return;

  float speed = 1.0f / app_fade_duration_;

  if (app_fade_in_) {
    app_fade_progress_ += speed * dt;
    if (app_fade_progress_ >= 1.0f) {
      app_fade_progress_ = 1.0f;
    }
  } else {
    app_fade_progress_ -= speed * dt;
    if (app_fade_progress_ <= 0.0f) {
      app_fade_progress_ = 0.0f;
      app_fade_active_ = false;
    }
  }
}

void Application::renderAppFade() {
  if (!app_fade_active_ && app_fade_progress_ <= 0.0f)
    return;
  ImGuiIO &io = ImGui::GetIO();
  ImDrawList *dl = ImGui::GetForegroundDrawList();
  int alpha = (int)(255.0f * app_fade_progress_);
  dl->AddRectFilled(ImVec2(0, 0), ImVec2(io.DisplaySize.x, io.DisplaySize.y),
                    IM_COL32(0, 0, 0, alpha));
}

void Application::handleBtEvent(const BtEvent &ev) {
  shell_.invalidatePanels(); // invalidar specs cacheados
  void *ic = shell_.ui_icons.bluetooth.get();
  auto &T = shell_.toasts;
  const std::string name = ev.name.empty() ? ev.mac : ev.name;
  switch (ev.type) {
  case BtEventType::ScanStarted:
    T.push(ic, _("BLUETOOTH"), _("SCANNING..."));
    break;
  case BtEventType::ScanFinished:
    T.push(ic, _("BLUETOOTH"), _("SCAN FINISHED"));
    break;
  case BtEventType::ConnectOk:
    T.push(ic, _("BLUETOOTH"), _("CONNECTED: ") + name);
    bt_rescan_pending_ = true;
    bt_rescan_time_ = SDL_GetPerformanceCounter();
    break;
  case BtEventType::ConnectFailed:
    T.push(ic, _("BLUETOOTH"), _("CONNECTION FAILED: ") + name, 5.0f);
    break;
  case BtEventType::ConnectNeedsPin:
  case BtEventType::PairNeedsPin:
    T.push(ic, _("BLUETOOTH"), _("PIN REQUIRED: ") + name);
    shell_.show_pin = true;
    shell_.pin_mac = ev.mac;
    shell_.pin_name = name;
    shell_.pin_buffer.clear();
    shell_.pin_focus = 1; // primera tecla numérica
    shell_.pin_scroll = 0.0f;
    break;
  case BtEventType::PairOk:
    T.push(ic, _("BLUETOOTH"), _("PAIRED: ") + name);
    break;
  case BtEventType::PairFailed:
    if (ev.detail != "cancelled")
      T.push(ic, _("BLUETOOTH"), _("PAIRING FAILED: ") + name, 5.0f);
    break;
  case BtEventType::DisconnectOk:
    T.push(ic, _("BLUETOOTH"), _("DISCONNECTED: ") + name);
    bt_rescan_pending_ = true;
    bt_rescan_time_ = SDL_GetPerformanceCounter();
    break;
  case BtEventType::DisconnectFailed:
    T.push(ic, _("BLUETOOTH"), _("DISCONNECT FAILED: ") + name, 5.0f);
    break;
  case BtEventType::RemoveOk:
    T.push(ic, _("BLUETOOTH"), _("UNLINKED: ") + name);
    bt_rescan_pending_ = true;
    bt_rescan_time_ = SDL_GetPerformanceCounter();
    break;
  case BtEventType::RemoveFailed:
    T.push(ic, _("BLUETOOTH"), _("UNLINK FAILED: ") + name, 5.0f);
    break;
  case BtEventType::DevicesChanged:
    break;
  }
}