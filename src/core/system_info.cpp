#include "system_info.h"
#include "build_info.h" // generado por CMake
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <unistd.h>

namespace {
static std::string lowerStr(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return s;
}

// Infiere el vendor a partir del modelo, por si la línea cruda no lo trae
static const char *vendorFromModel(const std::string &m) {
    // NVIDIA: familias históricas y actuales
    if (m.find("geforce") != std::string::npos ||
        m.find("quadro")  != std::string::npos ||
        m.find("tesla")   != std::string::npos ||
        m.find("nvs")     != std::string::npos ||
        m.find("titan")   != std::string::npos ||
        m.find("rtx")     != std::string::npos ||
        m.find("gtx")     != std::string::npos ||
        m.find("gts ")    != std::string::npos ||
        m.find("gt ")     != std::string::npos)
        return "NVIDIA";
    // AMD
    if (m.find("radeon")  != std::string::npos ||
        m.find("firepro") != std::string::npos ||
        m.find("vega")    != std::string::npos ||
        m.find("navi")    != std::string::npos)
        return "AMD";
    // Intel
    if (m.find("hd graphics") != std::string::npos ||
        m.find("uhd")  != std::string::npos ||
        m.find("iris") != std::string::npos ||
        m.find("arc")  != std::string::npos)
        return "Intel";
    return nullptr;
}

static std::string prettifyGpu(const std::string &raw) {
    std::string s = raw;

    // quitar "(rev a1)" y similares
    auto rev = s.find("(rev ");
    if (rev != std::string::npos)
        s = s.substr(0, rev);

    // 1) Vendor desde la línea cruda ("NVIDIA Corporation ...", etc.)
    const char *vendor = nullptr;
    std::string lr = lowerStr(s);
    if (lr.find("nvidia") != std::string::npos)
        vendor = "NVIDIA";
    else if (lr.find("amd") != std::string::npos ||
             lr.find("ati") != std::string::npos)
        vendor = "AMD";
    else if (lr.find("intel") != std::string::npos)
        vendor = "Intel";

    // 2) Preferir el nombre comercial entre [ ]
    std::string model;
    auto b = s.find('[');
    auto e = s.find(']');
    if (b != std::string::npos && e != std::string::npos && e > b + 1) {
        model = s.substr(b + 1, e - b - 1);
    } else {
        model = s;
        while (!model.empty() && model.back() == ' ')
            model.pop_back();
    }

    // 3) Si la línea cruda no traía vendor, inferirlo del modelo
    if (!vendor)
        vendor = vendorFromModel(lowerStr(model));

    // 4) Preponer el vendor si el modelo no lo incluye ya
    if (vendor && lowerStr(model).find(lowerStr(vendor)) == std::string::npos)
        model = std::string(vendor) + " " + model;

    return model;
}
std::string readFileTrim(const std::string &path) {
  std::ifstream f(path);
  if (!f)
    return "";
  std::stringstream ss;
  ss << f.rdbuf();
  std::string s = ss.str();
  while (!s.empty() &&
         (s.back() == '\n' || s.back() == '\r' || s.back() == ' '))
    s.pop_back();
  return s;
}

std::string runCmd(const std::string &cmd) {
  std::array<char, 256> buf;
  std::string out;
  FILE *p = popen(cmd.c_str(), "r");
  if (!p)
    return "";
  while (fgets(buf.data(), (int)buf.size(), p))
    out += buf.data();
  pclose(p);
  while (!out.empty() && (out.back() == '\n' || out.back() == '\r'))
    out.pop_back();
  return out;
}

std::string readCpu() {
  std::ifstream f("/proc/cpuinfo");
  std::string line;
  while (std::getline(f, line)) {
    if (line.rfind("model name", 0) == 0) {
      auto pos = line.find(':');
      if (pos != std::string::npos) {
        std::string v = line.substr(pos + 1);
        while (!v.empty() && v.front() == ' ')
          v.erase(v.begin());
        return v;
      }
    }
  }
  return "Desconocido";
}

std::string readGpu() {
  // lspci filtra la VGA/3D controller; fallback a sysfs
  std::string out =
      runCmd("lspci 2>/dev/null | grep -E 'VGA|3D|Display' | head -n1");
  if (!out.empty()) {
    auto pos = out.find(": ");
    if (pos != std::string::npos)
      return out.substr(pos + 2);
    return out;
  }
  return readFileTrim("/sys/class/drm/card0/device/vendor").empty()
             ? "Desconocido"
             : "GPU (ver /sys/class/drm/)";
}

std::string readRam() {
  std::ifstream f("/proc/meminfo");
  std::string line;
  while (std::getline(f, line)) {
    if (line.rfind("MemTotal:", 0) == 0) {
      long kb = 0;
      std::sscanf(line.c_str(), "MemTotal: %ld kB", &kb);
      char buf[32];
      std::snprintf(buf, sizeof(buf), "%.1f GiB", kb / (1024.0 * 1024.0));
      return buf;
    }
  }
  return "Desconocido";
}

std::string readVram() {
  // AMDGPU expone VRAM en sysfs. Intel/NVIDIA no de forma estándar.
  std::string v = readFileTrim("/sys/class/drm/card0/mem_info_vram_total");
  if (!v.empty()) {
    long long bytes = std::strtoll(v.c_str(), nullptr, 10);
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.1f GiB",
                  bytes / (1024.0 * 1024.0 * 1024.0));
    return buf;
  }
  return "No disponible";
}

std::string readOs() {
  std::ifstream f("/etc/os-release");
  std::string line;
  while (std::getline(f, line)) {
    if (line.rfind("PRETTY_NAME=", 0) == 0) {
      std::string v = line.substr(12);
      if (!v.empty() && v.front() == '"')
        v.erase(0, 1);
      if (!v.empty() && v.back() == '"')
        v.pop_back();
      return v;
    }
  }
  return "Linux";
}

std::string readDesktop() {
  if (const char *d = std::getenv("XDG_CURRENT_DESKTOP"); d && *d)
    return d;
  if (const char *d = std::getenv("DESKTOP_SESSION"); d && *d)
    return d;
  if (const char *d = std::getenv("XDG_SESSION_TYPE"); d && *d)
    return d;
  return "Desconocido";
}

} // namespace

SystemInfo SystemInfo::collect() {
  SystemInfo s;
  s.cpu = readCpu();
  s.gpu = prettifyGpu(readGpu());
  s.ram = readRam();
  s.vram = readVram();
  s.os = readOs();
  s.desktop = readDesktop();
  s.git_commit = LUDEX_GIT_COMMIT;
  s.build_date = std::string(LUDEX_BUILD_DATE) + " " + LUDEX_BUILD_TIME;
  return s;
}