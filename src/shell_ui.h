#pragma once

#include <functional>

#include "app_discovery.h"
#include "config.h"
#include "shell_state.h"
#include "input_manager.h"

struct ShellActions {
    std::function<void(const App&)> launch;
    std::function<void()> open_settings;
    std::function<void()> quit;
    std::function<void()> poweroff;
    std::function<void()> reboot;
    std::function<void()> suspend;
    std::function<std::vector<InputManager::PlayerStatus>()> player_status;
    std::function<void()> reload_ui_icons;
    std::function<void()> open_controllers;
    std::function<void()> apply_controllers;
    std::function<std::vector<InputManager::DeviceInfo>()> devices;
};

void panelInput(ShellState& st, Config& cfg, const ShellActions& actions, UiAction a);
void controllersInput(ShellState& st, Config& cfg, const ShellActions& actions, UiAction a);

void loadShellFonts(const Config& cfg, float screen_h);

void drawShellImGui(
    ShellState& state,
    const Config& cfg,
    const ShellActions& actions
);