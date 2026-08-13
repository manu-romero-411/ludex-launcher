#pragma once

#include <functional>

#include "app_discovery.h"
#include "shell_state.h"

void drawShellImGui(
    const ShellState& state,
    const std::function<void(const App&)>& on_launch
);
