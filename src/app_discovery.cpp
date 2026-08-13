#include "app_discovery.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>

static std::string trim(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) {
        return "";
    }

    size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

std::string displayNameFromStem(std::string stem) {
    for (char& c : stem) {
        if (c == '_' || c == '-') {
            c = ' ';
        }
    }

    bool capitalize_next = true;

    for (char& c : stem) {
        unsigned char uc = static_cast<unsigned char>(c);

        if (std::isspace(uc)) {
            capitalize_next = true;
        } else {
            if (capitalize_next) {
                c = static_cast<char>(std::toupper(uc));
                capitalize_next = false;
            } else {
                c = static_cast<char>(std::tolower(uc));
            }
        }
    }

    return stem;
}

std::vector<std::string> makeWebappCommand(
    const Config& cfg,
    const std::string& url
) {
    std::vector<std::string> cmd;

    cmd.push_back(cfg.browser_bin);

    for (const auto& arg : cfg.browser_extra_args) {
        cmd.push_back(arg);
    }

    cmd.push_back("--app=" + url);

    return cmd;
}

static std::optional<std::filesystem::path> findIcon(
    const std::filesystem::path& apps_dir,
    const std::string& stem,
    const std::vector<std::string>& icon_exts
) {
    const std::filesystem::path icons_dir = apps_dir / "app-icons";

    for (const auto& ext : icon_exts) {
        std::filesystem::path candidate = icons_dir / (stem + ext);

        std::error_code ec;
        if (std::filesystem::is_regular_file(candidate, ec)) {
            return candidate;
        }
    }

    return std::nullopt;
}

static std::vector<App> discoverWebapps(const Config& cfg) {
    std::vector<App> apps;

    std::error_code ec;
    if (!std::filesystem::is_directory(cfg.apps_dir, ec)) {
        std::cerr << "[tenfoot-shell] APPS_DIR no existe: "
                  << cfg.apps_dir << std::endl;
        return apps;
    }

    std::vector<std::filesystem::path> entries;

    for (const auto& entry :
         std::filesystem::directory_iterator(cfg.apps_dir, ec)) {
        if (entry.is_regular_file(ec) && entry.path().extension() == ".webapp") {
            entries.push_back(entry.path());
        }
    }

    std::sort(entries.begin(), entries.end());

    for (const auto& path : entries) {
        std::ifstream f(path);
        if (!f) {
            std::cerr << "[tenfoot-shell] no se pudo leer "
                      << path << std::endl;
            continue;
        }

        std::stringstream buffer;
        buffer << f.rdbuf();

        std::string url = trim(buffer.str());

        if (url.empty()) {
            std::cerr << "[tenfoot-shell] " << path
                      << " está vacío, se ignora" << std::endl;
            continue;
        }

        App app;
        app.name = displayNameFromStem(path.stem().string());
        app.cmd = makeWebappCommand(cfg, url);

        auto icon = findIcon(cfg.apps_dir, path.stem().string(), cfg.icon_exts);
        if (icon) {
            app.icon_path = *icon;
        }

        apps.push_back(std::move(app));
    }

    std::sort(
        apps.begin(),
        apps.end(),
        [](const App& a, const App& b) {
            std::string an = a.name;
            std::string bn = b.name;

            std::transform(
                an.begin(),
                an.end(),
                an.begin(),
                [](unsigned char c) {
                    return std::tolower(c);
                }
            );

            std::transform(
                bn.begin(),
                bn.end(),
                bn.begin(),
                [](unsigned char c) {
                    return std::tolower(c);
                }
            );

            return an < bn;
        }
    );

    return apps;
}

std::vector<App> discoverApps(const Config& cfg) {
    std::vector<App> apps;

    for (const auto& native : cfg.native_apps) {
        App app;
        app.name = native.name;
        app.cmd = native.cmd;
        apps.push_back(std::move(app));
    }

    std::vector<App> webapps = discoverWebapps(cfg);

    apps.insert(
        apps.end(),
        std::make_move_iterator(webapps.begin()),
        std::make_move_iterator(webapps.end())
    );

    return apps;
}
