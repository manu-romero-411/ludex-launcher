#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "config.h"

struct App {
    std::string name;
    std::vector<std::string> cmd;
    std::filesystem::path icon_path;
    void* icon_texture = nullptr;
};

std::string displayNameFromStem(std::string stem);

std::vector<std::string> makeWebappCommand(
    const Config& cfg,
    const std::string& url
);

std::vector<App> discoverApps(const Config& cfg);
