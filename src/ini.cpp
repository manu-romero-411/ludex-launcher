#include "ini.h"
#include "util.h"

#include <fstream>

bool Ini::load(const std::filesystem::path& path) {    std::ifstream f(path);
    if (!f) return false;

    std::string line;
    std::string section;

    while (std::getline(f, line)) {
        std::string t = util::trim(line);

        if (t.empty() || t[0] == ';' || t[0] == '#') continue;

        if (t.front() == '[' && t.back() == ']') {
            section = util::trim(t.substr(1, t.size() - 2));
            continue;
        }

        size_t eq = t.find('=');
        if (eq == std::string::npos) continue;

        std::string key = util::trim(t.substr(0, eq));
        std::string val = util::trim(t.substr(eq + 1));

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
    try { return std::stoi(v); } catch (...) { return def; }
}

float Ini::getFloat(const std::string& s, const std::string& k, float def) const {
    std::string v = get(s, k);
    if (v.empty()) return def;
    try { return std::stof(v); } catch (...) { return def; }
}

void Ini::set(const std::string& s, const std::string& k, const std::string& v) {
    data_[s][k] = v;
}