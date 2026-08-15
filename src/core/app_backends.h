#pragma once

#include <filesystem>
#include <unordered_map>
#include <string>
#include <vector>

struct Backend {
    std::string name;           // identificador snake_case
    std::string exec_start;     // plantilla de comando con placeholders
    std::string exec_end;       // comando de aborto (reservado, futuro)
    std::filesystem::path source_path;
};

class BackendRegistry {
public:
    void loadAll();             // escanea todos los directorios, en prioridad
    const Backend* find(const std::string& name) const;
    size_t size() const { return backends_.size(); }

private:
    void loadDir(const std::filesystem::path& dir);
        std::unordered_map<std::string, Backend> backends_;
};

/* Sustituye %URL%/%RUN%, %APP% y %CONTROLLERSCONFIG% en exec_start
 * y tokeniza respetando comillas. */
std::vector<std::string> buildBackendCommand(
    const Backend& backend,
    const std::string& run,
    const std::filesystem::path& webapp_path,
    const std::string& controllers_config,                    // estilo ES inline
    const std::filesystem::path& controllers_file = {});      // %CONTROLLERSFILE%