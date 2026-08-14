// src/util.h
#pragma once
#include <string>
#include <vector>
#include <filesystem>

namespace util {
std::string trim(const std::string& s);
std::vector<std::string> splitCsv(const std::string& s);
std::string joinCsv(const std::vector<std::string>& v);
std::filesystem::path exeDir();   // /proc/self/exe
std::string quoteIfNeeded(const std::string& s);
}
