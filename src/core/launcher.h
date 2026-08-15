#pragma once

#include <functional>
#include <string>
#include <vector>

struct LaunchHooks {
    std::function<void()> before;
    std::function<void()> after;
};

int launchApp(
    const std::vector<std::string>& cmd,
    const LaunchHooks& hooks
);