#include <SDL.h>
#include <SDL_vulkan.h>

#include <iostream>
#include <memory>

#include "app_discovery.h"
#include "config.h"
#include "input_manager.h"
#include "ir_input.h"
#include "launcher.h"
#include "renderer.h"
#include "shell_state.h"

static void handleIrEvent(
    const IrEvent& event,
    ShellState& shell,
    bool& running
) {
    const std::string& code = event.code;

    if (code == "KEY_LEFT" || code == "LEFT" || code == "left") {
        shell.nav(-1);
    } else if (code == "KEY_RIGHT" || code == "RIGHT" || code == "right") {
        shell.nav(1);
    } else if (code == "KEY_ENTER" || code == "OK" || code == "SELECT" ||
               code == "enter" || code == "ok" || code == "select") {
        if (const App* app = shell.selectedApp()) {
            LaunchHooks hooks;
            launchApp(app->cmd, hooks);
        }
    } else if (code == "KEY_ESC" || code == "BACK" || code == "EXIT" ||
               code == "esc" || code == "back" || code == "exit") {
        running = false;
    }
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0) {
        std::cerr << "SDL_Init error: " << SDL_GetError() << std::endl;
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow(
        "tenfoot-shell",
        SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED,
        1280,
        720,
        SDL_WINDOW_VULKAN |
        SDL_WINDOW_FULLSCREEN_DESKTOP |
        SDL_WINDOW_ALLOW_HIGHDPI
    );

    if (!window) {
        std::cerr << "SDL_CreateWindow error: " << SDL_GetError() << std::endl;
        SDL_Quit();
        return 1;
    }

    Config cfg = loadConfig();

    InputManager input;
    if (!input.init()) {
        std::cerr << "No se pudo inicializar InputManager" << std::endl;
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    auto ir = createDefaultIrInput();
    if (!ir->init()) {
        std::cerr << "[tenfoot-shell] IR no disponible" << std::endl;
    }

    ShellState shell;
    shell.refresh(cfg);

    std::unique_ptr<Renderer> renderer = createVulkanRenderer();

    if (!renderer->init(window)) {
        std::cerr << "No se pudo inicializar el renderer" << std::endl;
        input.shutdown();
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    bool running = true;

    Uint64 last_time = SDL_GetPerformanceCounter();

    while (running) {
        Uint64 now_time = SDL_GetPerformanceCounter();

        float dt = static_cast<float>(
            static_cast<double>(now_time - last_time) /
            static_cast<double>(SDL_GetPerformanceFrequency())
        );

        last_time = now_time;

        SDL_Event event;

        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;
            }

            if (event.type == SDL_KEYDOWN) {
                switch (event.key.keysym.sym) {
                    case SDLK_LEFT:
                    case SDLK_a:
                        shell.nav(-1);
                        break;

                    case SDLK_RIGHT:
                    case SDLK_d:
                        shell.nav(1);
                        break;

                    case SDLK_RETURN:
                    case SDLK_SPACE: {
                        if (const App* app = shell.selectedApp()) {
                            LaunchHooks hooks;

                            hooks.before = [&]() {
                                input.closeControllers();
                                SDL_MinimizeWindow(window);
                                SDL_HideWindow(window);
                                SDL_FlushEvents(SDL_FIRSTEVENT, SDL_LASTEVENT);
                            };

                            hooks.after = [&]() {
                                input.rescanControllers();
                                SDL_ShowWindow(window);
                                SDL_RaiseWindow(window);
                                SDL_FlushEvents(SDL_FIRSTEVENT, SDL_LASTEVENT);
                            };

                            launchApp(app->cmd, hooks);
                        }
                        break;
                    }

                    case SDLK_ESCAPE:
                        running = false;
                        break;

                    case SDLK_F5:
                        shell.refresh(cfg);
                        break;

                    case SDLK_F8: {
                        // Debug: mover J1 hacia J2.
                        input.movePlayer(0, 1);
                        break;
                    }

                    case SDLK_F9: {
                        // Debug: mover J2 hacia J1.
                        input.movePlayer(1, 0);
                        break;
                    }

                    case SDLK_F10: {
                        auto players = input.describePlayers();
                        for (const auto& line : players) {
                            SDL_Log("%s", line.c_str());
                        }
                        break;
                    }

                    default:
                        break;
                }
            }

            input.handleEvent(event);
        }

        UiInput ui;

        while (input.poll(ui)) {
            if (ui.player != cfg.active_player) {
                continue;
            }

            switch (ui.action) {
                case UiAction::Left:
                    shell.nav(-1);
                    break;

                case UiAction::Right:
                    shell.nav(1);
                    break;

                case UiAction::Select: {
                    if (const App* app = shell.selectedApp()) {
                        LaunchHooks hooks;

                        hooks.before = [&]() {
                            input.closeControllers();
                            SDL_MinimizeWindow(window);
                            SDL_HideWindow(window);
                            SDL_FlushEvents(SDL_FIRSTEVENT, SDL_LASTEVENT);
                        };

                        hooks.after = [&]() {
                            input.rescanControllers();
                            SDL_ShowWindow(window);
                            SDL_RaiseWindow(window);
                            SDL_FlushEvents(SDL_FIRSTEVENT, SDL_LASTEVENT);
                        };

                        launchApp(app->cmd, hooks);
                    }
                    break;
                }

                case UiAction::Back:
                    running = false;
                    break;

                case UiAction::Menu:
                    // Aquí podrás abrir un menú de configuración,
                    // reordenación de mandos, etc.
                    break;
            }
        }

        if (auto ir_event = ir->poll()) {
            handleIrEvent(*ir_event, shell, running);
        }

        shell.update(dt);

        renderer->beginFrame();

        renderer->drawShell(
            shell,
            [&](const App& app) {
                LaunchHooks hooks;

                hooks.before = [&]() {
                    input.closeControllers();
                    SDL_MinimizeWindow(window);
                    SDL_HideWindow(window);
                    SDL_FlushEvents(SDL_FIRSTEVENT, SDL_LASTEVENT);
                };

                hooks.after = [&]() {
                    input.rescanControllers();
                    SDL_ShowWindow(window);
                    SDL_RaiseWindow(window);
                    SDL_FlushEvents(SDL_FIRSTEVENT, SDL_LASTEVENT);
                };

                launchApp(app.cmd, hooks);
            }
        );

        renderer->endFrame();
    }

    renderer->shutdown();
    ir->shutdown();
    input.shutdown();

    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
