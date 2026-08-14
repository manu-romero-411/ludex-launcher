// application.h
#pragma once
#include "config.h"
#include "shell_state.h"
#include "backends.h"
#include "input_manager.h"
#include "audio_manager.h"
#include "renderer.h"
#include "shell_ui.h"
#include "ir_input.h"
#include <memory>

class Application {
public:
    bool init();
    int run();
    void shutdown();

private:
    void setupAudio();
    void setupActions();
    void processEvents(float dt);
    void handleAction(UiAction a);
    void handleKeyboard(const SDL_Event& e);
    void handleMouseDrag(const SDL_Event& e);
    void handleTouchDrag(const SDL_Event& e);

    SDL_Window* window_ = nullptr;
    Config cfg_;
    ShellState shell_;
    BackendRegistry backends_;
    InputManager input_;
    AudioManager audio_;
    std::unique_ptr<Renderer> renderer_;
    std::unique_ptr<IrInput> ir_;
    ShellActions actions_;
    bool running_ = false;
    bool app_running_ = false;
    int sw_ = 0, sh_ = 0;
};