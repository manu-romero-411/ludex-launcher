#include "backends.h"

#include <SDL.h>

#include <cctype>
#include <cstdlib>
#include <iostream>
#include <unistd.h>

#include "ini.h"

static std::string quoteIfNeeded(const std::string& s) {
    if (s.find_first_of(" \t") == std::string::npos) return s;
    return "\"" + s + "\"";
}

static std::string replaceAll(std::string s,
                              const std::string& from, const std::string& to) {
    if (from.empty()) return s;
    size_t pos = 0;
    while ((pos = s.find(from, pos)) != std::string::npos) {
        s.replace(pos, from.size(), to);
        pos += to.size();
    }
    return s;
}

/* Tokenizador con comillas simples/dobles (sin shell, sin globs: seguro) */
static std::vector<std::string> tokenize(const std::string& s) {
    std::vector<std::string> out;
    std::string cur;
    bool in_s = false, in_d = false, has = false;

    for (char c : s) {
        if (in_s) {
            if (c == '\'') in_s = false; else cur += c;
        } else if (in_d) {
            if (c == '"') in_d = false; else cur += c;
        } else if (c == '\'') { in_s = true; has = true; }
        else if (c == '"')    { in_d = true; has = true; }
        else if (std::isspace((unsigned char)c)) {
            if (!cur.empty() || has) { out.push_back(cur); cur.clear(); has = false; }
        } else cur += c;
    }
    if (!cur.empty() || has) out.push_back(cur);
    return out;
}

static std::filesystem::path exeDir() {
    char buf[4096];
    ssize_t n = ::readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (n <= 0) return {};
    buf[n] = '\0';
    return std::filesystem::path(buf).parent_path();
}

void BackendRegistry::loadAll() {
    backends_.clear();

    // Orden de prioridad (los últimos pisan a los anteriores):
    // sistema < usuario < directorio del exe < cwd < env
    std::vector<std::filesystem::path> dirs;
    dirs.push_back("/usr/share/ludex-launcher/backends");

    if (const char* xdg = std::getenv("XDG_CONFIG_HOME")) {
        dirs.push_back(std::filesystem::path(xdg) / "ludex" / "backends");
    } else if (const char* home = std::getenv("HOME")) {
        dirs.push_back(std::filesystem::path(home) / ".config" / "ludex" / "backends");
    }

    std::filesystem::path ed = exeDir();
    if (!ed.empty()) dirs.push_back(ed / "backends");
    dirs.push_back(std::filesystem::current_path() / "backends");

    if (const char* env = std::getenv("LUDEX_BACKENDS_DIR")) dirs.push_back(env);

    for (const auto& d : dirs) loadDir(d);

    SDL_Log("[ludex] %d backends cargados", (int)backends_.size());
}

void BackendRegistry::loadDir(const std::filesystem::path& dir) {
    std::error_code ec;
    if (!std::filesystem::is_directory(dir, ec)) return;

    for (const auto& e : std::filesystem::directory_iterator(dir, ec)) {
        if (!e.is_regular_file(ec) || e.path().extension() != ".backend") continue;

        Ini ini;
        if (!ini.load(e.path())) continue;

        const std::string S = "backend";
        Backend b;
        b.name         = ini.get(S, "name", e.path().stem().string());
        b.exec_start   = ini.get(S, "exec_start", "");
        b.exec_end     = ini.get(S, "exec_end", "");
        b.source_path  = e.path();

        if (b.exec_start.empty()) {
            std::cerr << "[ludex] backend '" << b.name
                      << "' sin exec_start, se ignora: " << e.path() << "\n";
            continue;
        }

        backends_[b.name] = b;
    }
}

const Backend* BackendRegistry::find(const std::string& name) const {
    auto it = backends_.find(name);
    return it == backends_.end() ? nullptr : &it->second;
}

std::vector<std::string> buildBackendCommand(
    const Backend& backend,
    const std::string& run,
    const std::filesystem::path& webapp_path,
    const std::string& controllers_config,
    const std::filesystem::path& controllers_file)
{
    std::string cmd = backend.exec_start;

    cmd = replaceAll(cmd, "%URL%", run);
    cmd = replaceAll(cmd, "%RUN%", run);
    cmd = replaceAll(cmd, "%APP%", quoteIfNeeded(webapp_path.string()));
    cmd = replaceAll(cmd, "%CONTROLLERSCONFIG%", controllers_config);
    cmd = replaceAll(cmd, "%CONTROLLERSFILE%",
                     controllers_file.empty()
                         ? "" : quoteIfNeeded(controllers_file.string()));

    return tokenize(cmd);
}
