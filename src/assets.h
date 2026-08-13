#pragma once

#include "config.h"

class Renderer;
struct ShellState;

void loadShellAssets(Renderer& renderer, ShellState& state, const Config& cfg);
void loadWallpaper(Renderer& renderer, ShellState& state, const Config& cfg);
void freeShellAssets(Renderer& renderer, ShellState& state);