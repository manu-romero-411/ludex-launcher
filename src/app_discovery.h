#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "config.h"

struct App {
    std::string name;
    std::vector<std::string> cmd;
    std::filesystem::path icon_path;
    int icon_texture_id = -1;
};

std::string displayNameFromStem(std::string stem);

std::vector<std::string> makeWebappCommand(
    const Config& cfg,
    const std::string& url
);

std::vector<App> discoverApps(const Config& cfg);
