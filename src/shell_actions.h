// src/shell_actions.h
#pragma once
#include <functional>
#include <vector>
#include "input_manager.h"

struct App;
struct ShellState;
struct Config;

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