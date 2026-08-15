#include "bluetooth_manager.h"
#include <SDL.h>
#include <array>
#include <cstdio>
#include <sstream>
#include <sys/wait.h>
#include <unistd.h>

namespace {
bool contains(const std::string &s, const char *k) {
  return s.find(k) != std::string::npos;
}

std::string lowerCopy(std::string s) {
  std::transform(s.begin(), s.end(), s.begin(),
                 [](unsigned char c) { return std::tolower(c); });
  return s;
}

std::string extractField(const std::string &out, const char *field) {
  std::istringstream stream(out);
  std::string line, prefix = std::string(field) + ": ";
  while (std::getline(stream, line)) {
    size_t start = line.find_first_not_of(" \t");
    if (start == std::string::npos)
      continue;
    if (line.compare(start, prefix.size(), prefix) == 0)
      return line.substr(start + prefix.size());
  }
  return "";
}

BtDeviceKind classifyByName(const std::string &name) {
  std::string n = lowerCopy(name);
  if (contains(n, "controller") || contains(n, "gamepad") ||
      contains(n, "joystick") || contains(n, "pro con"))
    return BtDeviceKind::Gamepad;
  if (contains(n, "headset") || contains(n, "headphone") ||
      contains(n, "earbud") || contains(n, "buds") || contains(n, "speaker") ||
      contains(n, "airpods"))
    return BtDeviceKind::Audio;
  return BtDeviceKind::Unknown;
}

BtDeviceKind classifyFromInfo(const std::string &info,
                              const std::string &name) {
  std::string icon = lowerCopy(extractField(info, "Icon"));
  if (!icon.empty()) {
    if (contains(icon, "gaming") || contains(icon, "gamepad") ||
        contains(icon, "joystick"))
      return BtDeviceKind::Gamepad;
    if (contains(icon, "audio") || contains(icon, "headset") ||
        contains(icon, "headphone") || contains(icon, "speaker") ||
        contains(icon, "earbud") || contains(icon, "mic"))
      return BtDeviceKind::Audio;
  }
  std::string cls = extractField(info, "Class");
  if (!cls.empty()) {
    try {
      uint32_t cod = (uint32_t)std::stoul(cls, nullptr, 0);
      uint32_t major = (cod >> 8) & 0x1F;
      if (major == 0x05)
        return BtDeviceKind::Gamepad;
      if (major == 0x04)
        return BtDeviceKind::Audio;
    } catch (...) {
    }
  }
  return classifyByName(name);
}
} // namespace

std::string BluetoothManager::runCmd(const std::string &cmd) {
  std::string result;
  std::array<char, 256> buffer;
  FILE *pipe = popen(cmd.c_str(), "r");
  if (!pipe)
    return result;
  while (fgets(buffer.data(), (int)buffer.size(), pipe) != nullptr)
    result += buffer.data();
  pclose(pipe);
  return result;
}

bool BluetoothManager::available() const {
  std::string out = runCmd("which bluetoothctl 2>/dev/null");
  return !out.empty();
}

void BluetoothManager::parseDevicesOutput(
    const std::string &output, std::vector<BluetoothDevice> &target) {
  target.clear();
  std::istringstream stream(output);
  std::string line;
  while (std::getline(stream, line)) {
    if (line.rfind("Device ", 0) != 0)
      continue;
    if (line.size() < 7 + 17)
      continue;
    std::string mac = line.substr(7, 17);
    std::string name = (line.size() > 25) ? line.substr(25) : "Unknown";
    while (!name.empty() &&
           (name.back() == '\r' || name.back() == '\n' || name.back() == ' '))
      name.pop_back();

    BluetoothDevice dev;
    dev.mac = mac;
    dev.name = name.empty() ? mac : name;
    dev.paired = true;
    dev.connected = false;
    target.push_back(dev);
  }
}

void BluetoothManager::refreshConnectedState() {
  for (auto &dev : devices_) {
    std::string out = runCmd("bluetoothctl info " + dev.mac + " 2>/dev/null");
    dev.connected = contains(out, "Connected: yes");
    dev.kind = classifyFromInfo(out, dev.name);
  }
}

void BluetoothManager::refresh() {
  std::string out = runCmd("bluetoothctl devices Paired 2>/dev/null");
  if (out.empty())
    out = runCmd("bluetoothctl paired-devices 2>/dev/null");
  parseDevicesOutput(out, devices_);
  refreshConnectedState();
}

void BluetoothManager::startScan(int timeout_sec) {
  if (scanning_)
    return;
  scan_timeout_sec_ = timeout_sec;
  scan_start_ = std::chrono::steady_clock::now();
  last_poll_ = scan_start_;
  scanning_ = true;
  discovered_devices_.clear();

  pid_t pid = fork();
  if (pid == 0) {
    freopen("/dev/null", "w", stdout);
    freopen("/dev/null", "w", stderr);
    std::string t = std::to_string(timeout_sec);
    execlp("bluetoothctl", "bluetoothctl", "--timeout", t.c_str(), "scan", "on",
           nullptr);
    _exit(127);
  }
  SDL_Log("[ludex] BT scan iniciado (%ds)", timeout_sec);
}

int BluetoothManager::scanRemainingSec() const {
  if (!scanning_)
    return 0;
  auto now = std::chrono::steady_clock::now();
  double elapsed = std::chrono::duration<double>(now - scan_start_).count();
  int rem = scan_timeout_sec_ - (int)elapsed;
  return rem > 0 ? rem : 0;
}

bool BluetoothManager::trust(const std::string &mac) {
  std::string out = runCmd("bluetoothctl trust " + mac + " 2>&1");
  SDL_Log("[ludex] BT trust %s: %s", mac.c_str(), out.c_str());
  return out.find("succeeded") != std::string::npos ||
         out.find("AlreadyExists") != std::string::npos;
}

bool BluetoothManager::pair(const std::string &mac) {
  std::string out = runCmd("bluetoothctl pair " + mac + " 2>&1");
  SDL_Log("[ludex] BT pair %s: %s", mac.c_str(), out.c_str());
  trust(mac);
  refresh();
  return out.find("successful") != std::string::npos ||
         out.find("AlreadyExists") != std::string::npos;
}

bool BluetoothManager::connect(const std::string &mac) {
  std::string out = runCmd("bluetoothctl connect " + mac + " 2>&1");
  SDL_Log("[ludex] BT connect %s: %s", mac.c_str(), out.c_str());
  trust(mac);
  refresh();
  return out.find("successful") != std::string::npos ||
         out.find("AlreadyExists") != std::string::npos;
}

bool BluetoothManager::disconnect(const std::string &mac) {
  std::string out = runCmd("bluetoothctl disconnect " + mac + " 2>&1");
  refresh();
  return out.find("successful") != std::string::npos;
}

bool BluetoothManager::removeDevice(const std::string &mac) {
  std::string out = runCmd("bluetoothctl remove " + mac + " 2>&1");
  refresh();
  return out.find("successful") != std::string::npos;
}

void BluetoothManager::setAutoRefresh(bool on) {
  if (on == auto_refresh_)
    return;
  auto_refresh_ = on;
  if (on) {
    refresh(); // refresco inmediato al abrir el panel
    last_refresh_ = std::chrono::steady_clock::now();
  }
}

void BluetoothManager::update() {
  auto now = std::chrono::steady_clock::now();

  // --- Escaneo activo ---
  if (scanning_) {
    double elapsed = std::chrono::duration<double>(now - scan_start_).count();
    if (elapsed >= (double)scan_timeout_sec_) {
      scanning_ = false;
      refresh();
      SDL_Log("[ludex] BT scan terminado, %d paired, %d discovered",
              (int)devices_.size(), (int)discovered_devices_.size());
      return;
    }

    double since_poll = std::chrono::duration<double>(now - last_poll_).count();
    if (since_poll >= 3.0) {
      last_poll_ = now;
      // `bluetoothctl devices` lista paired + descubiertos durante el scan
      std::string out = runCmd("bluetoothctl devices 2>/dev/null");
      std::vector<BluetoothDevice> all;
      parseDevicesOutput(out, all); // <-- 2 argumentos

      // discovered = los que NO están en la lista de paired
      discovered_devices_.clear();
      for (auto &dev : all) {
        bool is_paired = false;
        for (const auto &p : devices_) {
          if (p.mac == dev.mac) {
            is_paired = true;
            break;
          }
        }
        if (!is_paired) {
          dev.paired = false;
          dev.kind = classifyByName(dev.name);
          discovered_devices_.push_back(dev);
        }
      }
    }
    return;
  }

  // --- Auto-refresh: mantiene [CONNECTED] vivo mientras el panel está abierto
  // ---
  if (auto_refresh_) {
    double since = std::chrono::duration<double>(now - last_refresh_).count();
    if (since >= 2.0) {
      last_refresh_ = now;
      refresh();
    }
  }
}