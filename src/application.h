#pragma once

#include "audio_manager.h"
#include "backends.h"
#include "config.h"
#include "input_manager.h"
#include "ir_input.h"
#include "renderer.h"
#include "shell_state.h"
#include "shell_ui.h"
#include <SDL.h>
#include <memory>
#include <filesystem>

class Application {
public:
    bool init();
    int run();
    void shutdown();

private:
    void setupActions();
    void processEvents(float dt);
    void handleAction(UiAction a);
    
    void handleKeyboard(const SDL_Event& e);
    void handleMouseDrag(const SDL_Event& e);
    void handleTouchDrag(const SDL_Event& e);

    SDL_Surface* loadWindowIcon();
    std::filesystem::path runtimeDir();

    // Servicios
    SDL_Window* window_ = nullptr;
    Config cfg_;
    ShellState shell_;
    BackendRegistry backends_;
    InputManager input_;
    AudioManager audio_;
    std::unique_ptr<Renderer> renderer_;
    std::unique_ptr<IrInput> ir_;
    
    DragState drag_;
    ShellActions actions_;

    // Estado del bucle
    bool running_ = false;
    bool want_quit_ = false;
    bool app_running_ = false;
    int sw_ = 0, sh_ = 0;
    Uint64 last_time_ = 0;
};