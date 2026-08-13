#include "ini.h"

#include <cctype>
#include <fstream>

static std::string trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return "";
    size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

bool Ini::load(const std::filesystem::path& path) {
    std::ifstream f(path);
    if (!f) return false;

    std::string line;
    std::string section;

    while (std::getline(f, line)) {
        std::string t = trim(line);

        if (t.empty() || t[0] == ';' || t[0] == '#') continue;

        if (t.front() == '[' && t.back() == ']') {
            section = trim(t.substr(1, t.size() - 2));
            continue;
        }

        size_t eq = t.find('=');
        if (eq == std::string::npos) continue;

        std::string key = trim(t.substr(0, eq));
        std::string val = trim(t.substr(eq + 1));

        data_[section][key] = val;
    }

    return true;
}

bool Ini::save(const std::filesystem::path& path) const {
    std::error_code ec;
    if (path.has_parent_path()) {
        std::filesystem::create_directories(path.parent_path(), ec);
    }

    std::ofstream f(path);
    if (!f) return false;

    for (const auto& [section, kvs] : data_) {
        f << "[" << section << "]\n";
        for (const auto& [k, v] : kvs) {
            f << k << "=" << v << "\n";
        }
        f << "\n";
    }

    return true;
}

std::string Ini::get(const std::string& s, const std::string& k,
                     const std::string& def) const {
    auto it = data_.find(s);
    if (it == data_.end()) return def;
    auto jt = it->second.find(k);
    if (jt == it->second.end()) return def;
    return jt->second;
}

int Ini::getInt(const std::string& s, const std::string& k, int def) const {
    std::string v = get(s, k);
    if (v.empty()) return def;
    return std::atoi(v.c_str());
}

float Ini::getFloat(const std::string& s, const std::string& k, float def) const {
    std::string v = get(s, k);
    if (v.empty()) return def;
    return std::atof(v.c_str());
}

void Ini::set(const std::string& s, const std::string& k, const std::string& v) {
    data_[s][k] = v;
}