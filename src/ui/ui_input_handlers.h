#pragma once
#include "../core/config.h"
#include "../input/input_manager.h"
#include "shell/shell_state.h"

// Delegación de input a los paneles
void panelInput(ShellState& st, Config& cfg, const ShellActions& actions, UiAction a);
void controllersInput(ShellState& st, Config& cfg, const ShellActions& actions, UiAction a);