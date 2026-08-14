#include "application.h"
#include "assets.h"
#include "launcher.h"
#include "util.h"

#include <SDL_image.h>
#include <imgui_impl_sdl2.h>
#include <iostream>

std::filesystem::path Application::runtimeDir() {
    if (const char *rd = std::getenv("XDG_RUNTIME_DIR"))
        return std::filesystem::path(rd) / "ludex";
    return std::filesystem::path("/tmp") / "ludex";
}

SDL_Surface* Application::loadWindowIcon() {
    const char *names[] = {"ludex.svg", "ludex.png", "app.svg", "app.png"};
    for (const char *n : names) {
        std::filesystem::path p = cfg_.icons_dir / n;
        std::error_code ec;
        if (!std::filesystem::exists(p, ec)) continue;
        
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
        if (s) return s;
    }
    return nullptr;
}

bool Application::init() {
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
    SDL_SetHint(SDL_HINT_GAMECONTROLLER_USE_BUTTON_LABELS, "1");
    SDL_SetHint(SDL_HINT_APP_NAME, "ludex");

    const Uint32 sdl_flags = SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_GAMECONTROLLER;

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
                               SDL_WINDOW_VULKAN | SDL_WINDOW_FULLSCREEN_DESKTOP |
                               SDL_WINDOW_ALLOW_HIGHDPI);
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
        cfg_.music_dir,
        util::exeDir() / "music",
        std::filesystem::path(std::getenv("HOME") ? std::getenv("HOME") : "") / ".config" / "ludex" / "music",
        std::filesystem::path("/usr/share/ludex/music")
    };
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
    
    if (!input_.init()) {
        SDL_DestroyWindow(window_);
        SDL_Quit();
        return false;
    }

    ir_ = createDefaultIrInput();
    ir_->init();

    shell_.refresh(cfg_, backends_);

    renderer_ = createVulkanRenderer();
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

void Application::setupActions() {
    actions_.launch = [this](const App &app) {
        const Backend *b = backends_.find(app.backend);
        if (!b) {
            SDL_Log("[ludex] backend '%s' no disponible", app.backend.c_str());
            return;
        }

        std::string ccfg_str = input_.controllersConfigString();
        std::filesystem::path ccfg_file = input_.writeControllersConfig(runtimeDir());

        std::vector<std::string> cmd = buildBackendCommand(*b, app.run, app.webapp_path, ccfg_str, ccfg_file);
        if (cmd.empty()) return;

        LaunchHooks hooks;
        hooks.before = [this] {
            audio_.stopMusic();
            app_running_ = true;
            input_.closeControllers();
            renderer_->presentBlackFrame();
            SDL_FlushEvents(SDL_FIRSTEVENT, SDL_LASTEVENT);
        };
        hooks.after = [this] {
            drag_.reset();
            input_.clearTransientState();
            renderer_->getOutputSize(&sw_, &sh_);
            input_.rescanControllers();
            app_running_ = false;
            SDL_RaiseWindow(window_);
            SDL_FlushEvents(SDL_FIRSTEVENT, SDL_LASTEVENT);
            backends_.loadAll();
            shell_.refreshAndFreeOldAssets(*renderer_, cfg_, backends_);
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
        shell_.menu_open = false;
        shell_.settings_focus = 0;
    };
    actions_.quit = [this] { want_quit_ = true; };
    actions_.poweroff = [] { launchApp({"systemctl", "poweroff"}, LaunchHooks{}); };
    actions_.reboot = [] { launchApp({"systemctl", "reboot"}, LaunchHooks{}); };
    actions_.suspend = [] { launchApp({"systemctl", "suspend"}, LaunchHooks{}); };
    actions_.player_status = [this] { return input_.playerStatus(); };
    actions_.reload_ui_icons = [this] { loadUiIcons(*renderer_, shell_, cfg_, sh_); };
    
    actions_.open_controllers = [this] {
        shell_.show_controllers = true;
        shell_.menu_open = false;
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
}

int Application::run() {
    while (running_) {
        Uint64 now_time = SDL_GetPerformanceCounter();
        float dt = (float)((double)(now_time - last_time_) / (double)SDL_GetPerformanceFrequency());
        last_time_ = now_time;

        processEvents(dt);

        if (!app_running_) {
            input_.update();
            UiInput ui;
            while (input_.poll(ui)) {
                if (!cfg_.all_players_ui && ui.player != cfg_.active_player)
                    continue;
                handleAction(ui.action);
            }
        }

        if (!shell_.show_settings && !shell_.show_power && !shell_.show_controllers) {
            shell_.update(dt, cfg_);
        }
        audio_.update(); // Para procesar el flag de fin de música

        if (want_quit_) running_ = false;

        if (!app_running_ && shell_.pending_launch >= 0) {
            int idx = shell_.pending_launch;
            shell_.pending_launch = -1;
            if (idx >= 0 && idx < (int)shell_.apps.size()) {
                actions_.launch(shell_.apps[idx]);
                last_time_ = SDL_GetPerformanceCounter();
            }
        }

        renderer_->beginFrame();
        renderer_->drawShell(shell_, cfg_, actions_);
        renderer_->endFrame();
    }
    return 0;
}

void Application::shutdown() {
    audio_.shutdown();
    freeShellAssets(*renderer_, shell_);
    if (renderer_) renderer_->shutdown();
    if (ir_) ir_->shutdown();
    input_.shutdown();
    if (window_) SDL_DestroyWindow(window_);
    SDL_Quit();
}

void Application::processEvents(float dt) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT || 
           (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE)) {
            running_ = false;
            continue;
        }
        if (app_running_) continue;

        ImGui_ImplSDL2_ProcessEvent(&event);

        if (event.type == SDL_KEYDOWN) {
            handleKeyboard(event);
        } else if (event.type == SDL_MOUSEWHEEL) {
            if (!shell_.menu_open && !shell_.show_settings && !shell_.show_power && !shell_.show_controllers) {
                if (event.wheel.y != 0) {
                    int dy = (event.wheel.y > 0) ? -1 : 1;
                    shell_.nav(dy);
                    audio_.playScrollSound();
                }
            }
        }
        
        handleMouseDrag(event);
        handleTouchDrag(event);

        input_.handleEvent(event);
    }
}

void Application::handleKeyboard(const SDL_Event& e) {
    switch (e.key.keysym.sym) {
    case SDLK_UP: handleAction(UiAction::Up); break;
    case SDLK_DOWN: handleAction(UiAction::Down); break;
    case SDLK_LEFT: handleAction(UiAction::Left); break;
    case SDLK_RIGHT: handleAction(UiAction::Right); break;
    case SDLK_RETURN:
    case SDLK_SPACE: handleAction(UiAction::Select); break;
    case SDLK_ESCAPE: handleAction(UiAction::Back); break;
    case SDLK_F1:
    case SDLK_HOME: handleAction(UiAction::Guide); break;
    case SDLK_F5:
        backends_.loadAll();
        shell_.refreshAndFreeOldAssets(*renderer_, cfg_, backends_);
        {
            int icon_max = (int)(sh_ * cfg_.icon_sel_pct * 2.0f);
            for (auto &app : shell_.apps) {
                if (!app.icon_path.empty() && !app.icon_texture) {
                    app.icon_texture = renderer_->loadTextureFromFile(
                        app.icon_path, nullptr, nullptr, icon_max,
                        app.has_icon_tint ? &app.icon_tint : nullptr);
                }
            }
        }
        break;
    case SDLK_w: shell_.nextWallpaper(); break;
    case SDLK_F6: loadAllWallpapers(*renderer_, shell_, cfg_, sw_, sh_); break;
    default: break;
    }
}

void Application::handleMouseDrag(const SDL_Event& e) {
    static constexpr float DRAG_THRESHOLD = 12.0f;

    auto axisPos = [this](float px, float py) -> float {
        return (cfg_.side == "left" || cfg_.side == "right") ? py : px;
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
        float main_axis = (cfg_.side == "top" || cfg_.side == "bottom") ? (float)sw_ : (float)sh_;
        float slot_size = main_axis / ((float)(cfg_.visible_items - 1) + cfg_.tile_sel_ratio);

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
        if (!shell_.has_momentum) shell_.drag_velocity = 0.0f;
    };

    if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT) {
        if (!shell_.menu_open && !shell_.show_settings && !shell_.show_power && !shell_.show_controllers) {
            drag_.mouse_down = true;
            drag_.drag_active = false;
            drag_.mouse_down_x = (float)e.button.x;
            drag_.mouse_down_y = (float)e.button.y;
            startDragState(drag_.mouse_down_x, drag_.mouse_down_y);
        }
    }
    if (e.type == SDL_MOUSEBUTTONUP && e.button.button == SDL_BUTTON_LEFT) {
        if (drag_.drag_active) endDrag();
        drag_.mouse_down = false;
        drag_.drag_active = false;
    }
    if (e.type == SDL_MOUSEMOTION) {
        if (drag_.mouse_down && !drag_.drag_active) {
            float dx = (float)e.motion.x - drag_.mouse_down_x;
            float dy = (float)e.motion.y - drag_.mouse_down_y;
            if (std::sqrt(dx * dx + dy * dy) > DRAG_THRESHOLD) {
                drag_.drag_active = true;
                shell_.dragging = true;
            }
        }
        if (drag_.drag_active) {
            doDrag((float)e.motion.x, (float)e.motion.y);
        }
    }
}

void Application::handleTouchDrag(const SDL_Event& e) {
    static constexpr float DRAG_THRESHOLD = 12.0f;

    auto axisPos = [this](float px, float py) -> float {
        return (cfg_.side == "left" || cfg_.side == "right") ? py : px;
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
        float main_axis = (cfg_.side == "top" || cfg_.side == "bottom") ? (float)sw_ : (float)sh_;
        float slot_size = main_axis / ((float)(cfg_.visible_items - 1) + cfg_.tile_sel_ratio);

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
        if (!shell_.has_momentum) shell_.drag_velocity = 0.0f;
    };

    if (e.type == SDL_FINGERDOWN) {
        if (!shell_.menu_open && !shell_.show_settings && !shell_.show_power && !shell_.show_controllers) {
            drag_.finger_down = true;
            drag_.touch_drag_active = false;
            drag_.finger_down_x = e.tfinger.x * (float)sw_;
            drag_.finger_down_y = e.tfinger.y * (float)sh_;
            startDragState(drag_.finger_down_x, drag_.finger_down_y);
        }
    }
    if (e.type == SDL_FINGERUP) {
        if (drag_.touch_drag_active) endDrag();
        drag_.finger_down = false;
        drag_.touch_drag_active = false;
    }
    if (e.type == SDL_FINGERMOTION) {
        float fx = e.tfinger.x * (float)sw_;
        float fy = e.tfinger.y * (float)sh_;
        if (drag_.finger_down && !drag_.touch_drag_active) {
            float dx = fx - drag_.finger_down_x;
            float dy = fy - drag_.finger_down_y;
            if (std::sqrt(dx * dx + dy * dy) > DRAG_THRESHOLD) {
                drag_.touch_drag_active = true;
                shell_.dragging = true;
            }
        }
        if (drag_.touch_drag_active) {
            doDrag(fx, fy);
        }
    }
}

void Application::handleAction(UiAction a) {
    if (shell_.show_controllers) {
        controllersInput(shell_, cfg_, actions_, a);
        return;
    }
    if (shell_.show_settings || shell_.show_power) {
        panelInput(shell_, cfg_, actions_, a);
        return;
    }
    
    switch (a) {
    case UiAction::Left:
        if (shell_.menu_open) shell_.navMenu(-1);
        else if (cfg_.side == "top" || cfg_.side == "bottom") {
            shell_.nav(-1);
            audio_.playScrollSound();
        }
        break;
    case UiAction::Right:
        if (shell_.menu_open) shell_.navMenu(1);
        else if (cfg_.side == "top" || cfg_.side == "bottom") {
            shell_.nav(1);
            audio_.playScrollSound();
        }
        break;
    case UiAction::Up:
        if (shell_.menu_open) shell_.navMenu(-1);
        else if (cfg_.side == "left" || cfg_.side == "right") {
            shell_.nav(-1);
            audio_.playScrollSound();
        }
        break;
    case UiAction::Down:
        if (shell_.menu_open) shell_.navMenu(1);
        else if (cfg_.side == "left" || cfg_.side == "right") {
            shell_.nav(1);
            audio_.playScrollSound();
        }
        break;
    case UiAction::Select:
        audio_.playSelectSound();
        if (shell_.menu_open) {
            switch (shell_.menu_selected) {
            case 0: actions_.open_settings(); break;
            case 1: if (actions_.open_controllers) actions_.open_controllers(); break;
            case 2: want_quit_ = true; break;
            case 3:
            default:
                shell_.show_power = true;
                shell_.menu_open = false;
                shell_.power_focus = 0;
                break;
            }
        } else if (shell_.selectedApp()) {
            shell_.pending_launch = shell_.selected;
        }
        break;
    case UiAction::Back:
        if (shell_.menu_open) shell_.menu_open = false;
        break;
    case UiAction::Guide:
        shell_.menu_open = !shell_.menu_open;
        if (shell_.menu_open) shell_.menu_selected = 0;
        break;
    default: break;
    }
}