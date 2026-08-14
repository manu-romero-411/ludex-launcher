#include "util.h"
#include <cctype>
#include <sstream>
#include <unistd.h>

namespace util {

std::string trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return "";
    size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

std::vector<std::string> splitCsv(const std::string& s) {
    std::vector<std::string> out;
    std::istringstream iss(s);
    std::string cur;
    while (std::getline(iss, cur, ',')) {
        while (!cur.empty() && std::isspace((unsigned char)cur.front())) cur.erase(cur.begin());
        while (!cur.empty() && std::isspace((unsigned char)cur.back())) cur.pop_back();
        if (!cur.empty()) out.push_back(cur);
    }
    return out;
}

std::string joinCsv(const std::vector<std::string>& v) {
    std::string out;
    for (size_t i = 0; i < v.size(); ++i) {
        if (i) out += ",";
        out += v[i];
    }
    return out;
}

std::filesystem::path exeDir() {
    char buf[4096];
    ssize_t n = ::readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (n <= 0) return {};
    buf[n] = '\0';
    return std::filesystem::path(buf).parent_path();
}

std::string quoteIfNeeded(const std::string& s) {
    if (s.find_first_of(" \t") == std::string::npos) return s;
    return "\"" + s + "\"";
}

} // namespace util