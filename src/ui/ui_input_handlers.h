#pragma once
#include "../core/config.h"
#include "../input/input_manager.h"
#include "shell/shell_state.h"

// Delegación de input a los paneles
void controllersInput(ShellState& st, Config& cfg, const ShellActions& actions, UiAction a);
void panelInput(ShellState& st, Config& cfg, const ShellActions& actions, UiAction a);
