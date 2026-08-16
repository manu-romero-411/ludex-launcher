#pragma once
#include <string>

struct SystemInfo {
    std::string cpu;
    std::string gpu;
    std::string ram;      // "7.7 GiB"
    std::string vram;     // "Desconocido" o "1.5 GiB"
    std::string os;       // "Debian GNU/Linux 12 (bookworm)"
    std::string desktop;  // "sway" / "GNOME" / "X11"
    std::string git_commit;
    std::string build_date;

    static SystemInfo collect();
};