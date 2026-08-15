#pragma once
#include "../core/config.h"

class Renderer;
struct ShellState;

void loadShellAssets(Renderer& renderer, ShellState& state,
                     const Config& cfg, int screen_w, int screen_h);
void freeShellAssets(Renderer& renderer, ShellState& state);
void loadUiIcons(Renderer& renderer, ShellState& state,
                 const Config& cfg, int screen_h);