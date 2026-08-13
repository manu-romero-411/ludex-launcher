#pragma once

#include <filesystem>
#include <string>
#include <vector>

struct NativeApp {
    std::string name;
    std::vector<std::string> cmd;
};

struct Config {
    std::filesystem::path apps_dir =
        "/var/penguin/juegos/retrobox/resources/apps";

    std::filesystem::path wallpaper_dir =
        "/var/penguin/imagenes/fondos";

    std::vector<std::string> wallpaper_exts = {
        ".png",
        ".jpg",
        ".jpeg",
        ".webp"
    };

    std::vector<std::string> icon_exts = {
        ".png",
        ".jpg",
        ".jpeg",
        ".webp",
        ".svg"
    };

    std::string browser_bin = "google-chrome";

    std::vector<std::string> browser_extra_args = {
        "--new-window"
    };

    std::vector<NativeApp> native_apps = {
        {
            "Retrobox",
            {
                "/var/penguin/juegos/retrobox/resources/startup/retrobox_startup.py"
            }
        },
        {
            "Steam Big Picture",
            {
                "steam",
                "-tenfoot"
            }
        }
    };

    int active_player = 0;
};

Config loadConfig();
