#pragma once

#include <filesystem>
#include <map>
#include <string>

class Ini {
public:
    bool load(const std::filesystem::path& path);
    bool save(const std::filesystem::path& path) const;

    std::string get(const std::string& section, const std::string& key,
                    const std::string& def = "") const;
    int getInt(const std::string& s, const std::string& k, int def) const;
    float getFloat(const std::string& s, const std::string& k, float def) const;
    void set(const std::string& s, const std::string& k, const std::string& v);

private:
    std::map<std::string, std::map<std::string, std::string>> data_;
};