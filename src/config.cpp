#include "config.h"

#include <cstdlib>
#include <string>

static std::filesystem::path envPath(
    const char* name,
    const std::filesystem::path& fallback
) {
    const char* value = std::getenv(name);
    if (value && *value) {
        return value;
    }
    return fallback;
}

Config loadConfig() {
    Config cfg;

    cfg.apps_dir = envPath("TENFOOT_APPS_DIR", cfg.apps_dir);
    cfg.wallpaper_dir = envPath("TENFOOT_WALLPAPER_DIR", cfg.wallpaper_dir);

    if (const char* browser = std::getenv("TENFOOT_BROWSER_BIN")) {
        cfg.browser_bin = browser;
    }

    if (const char* active = std::getenv("TENFOOT_ACTIVE_PLAYER")) {
        cfg.active_player = std::atoi(active);
    }

    return cfg;
}
