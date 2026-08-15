#include "bluetooth_manager.h"
#include <SDL.h>
#include <algorithm>
#include <array>
#include <cstdio>
#include <poll.h>
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
      if (((cod >> 8) & 0x1F) == 0x05)
        return BtDeviceKind::Gamepad;
      if (((cod >> 8) & 0x1F) == 0x04)
        return BtDeviceKind::Audio;
    } catch (...) {
    }
  }
  return classifyByName(name);
}
std::string firstFailedLine(const std::string &out) {
  std::istringstream ss(out);
  std::string l;
  while (std::getline(ss, l))
    if (contains(l, "Failed") || contains(l, "failed"))
      return l;
  return "unknown error";
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

BluetoothManager::BluetoothManager() {
  worker_ = std::thread([this] { workerLoop(); });
}
BluetoothManager::~BluetoothManager() { shutdown(); }

void BluetoothManager::shutdown() {
  {
    std::lock_guard<std::mutex> l(mtx_);
    if (stop_)
      return;
    stop_ = true;
  }
  cv_.notify_all();
  pin_cancel_ = true;
  if (worker_.joinable())
    worker_.join();
  closePinSession();
}

bool BluetoothManager::available() const {
  if (available_cached_ < 0)
    available_cached_ =
        runCmd("which bluetoothctl 2>/dev/null").empty() ? 0 : 1;
  return available_cached_ == 1;
}

void BluetoothManager::enqueue(Request r) {
  {
    std::lock_guard<std::mutex> l(mtx_);
    queue_.push_back(std::move(r));
  }
  cv_.notify_all();
}
void BluetoothManager::requestRefresh() { enqueue({"refresh", "", ""}); }
void BluetoothManager::requestScan(int s) { enqueue({"scan", "", "", s}); }
void BluetoothManager::requestConnect(const std::string &m) {
  enqueue({"connect", m, ""});
}
void BluetoothManager::requestDisconnect(const std::string &m) {
  enqueue({"disconnect", m, ""});
}
void BluetoothManager::requestPair(const std::string &m) {
  enqueue({"pair", m, ""});
}
void BluetoothManager::requestRemove(const std::string &m) {
  enqueue({"remove", m, ""});
}

void BluetoothManager::submitPin(const std::string &mac,
                                 const std::string &pin) {
  bool wrote = false;
  {
    std::lock_guard<std::mutex> l(state_mtx_);
    if (awaiting_pin_ && pin_in_fd_ >= 0) {
      std::string s = pin + "\n";
      (void)::write(pin_in_fd_, s.data(), s.size());
      awaiting_pin_ = false;
      wrote = true;
    }
  }
  if (!wrote)
    enqueue({"pair_pin", mac, pin});
}
void BluetoothManager::cancelPin() {
  pin_cancel_ = true;
  {
    std::lock_guard<std::mutex> l(state_mtx_);
    awaiting_pin_ = false;
  }
}

void BluetoothManager::setAutoRefresh(bool on) {
  auto_refresh_ = on;
  if (on)
    enqueue({"refresh", "", ""});
}

bool BluetoothManager::pollEvent(BtEvent &out) {
  std::lock_guard<std::mutex> l(mtx_);
  if (events_.empty())
    return false;
  out = events_.front();
  events_.pop_front();
  return true;
}
void BluetoothManager::pushEvent(BtEvent e) {
  std::lock_guard<std::mutex> l(mtx_);
  events_.push_back(std::move(e));
}

std::vector<BluetoothDevice> BluetoothManager::devices() const {
  std::lock_guard<std::mutex> l(state_mtx_);
  return devices_;
}
std::vector<BluetoothDevice> BluetoothManager::discoveredDevices() const {
  std::lock_guard<std::mutex> l(state_mtx_);
  return discovered_;
}
bool BluetoothManager::isScanning() const {
  std::lock_guard<std::mutex> l(state_mtx_);
  return scanning_;
}
int BluetoothManager::scanRemainingSec() const {
  std::lock_guard<std::mutex> l(state_mtx_);
  if (!scanning_)
    return 0;
  auto now = std::chrono::steady_clock::now();
  int rem = scan_timeout_sec_ -
            (int)std::chrono::duration<double>(now - scan_start_).count();
  return rem > 0 ? rem : 0;
}
std::string BluetoothManager::nameFor(const std::string &mac) {
  std::lock_guard<std::mutex> l(state_mtx_);
  for (auto &d : devices_)
    if (d.mac == mac)
      return d.name;
  for (auto &d : discovered_)
    if (d.mac == mac)
      return d.name;
  return mac;
}

void BluetoothManager::parseDevicesOutput(
    const std::string &output, std::vector<BluetoothDevice> &target) {
  target.clear();
  std::istringstream stream(output);
  std::string line;
  while (std::getline(stream, line)) {
    if (line.rfind("Device ", 0) != 0 || line.size() < 24)
      continue;
    BluetoothDevice dev;
    dev.mac = line.substr(7, 17);
    dev.name = line.substr(25);
    while (!dev.name.empty() &&
           (dev.name.back() == '\r' || dev.name.back() == ' '))
      dev.name.pop_back();
    if (dev.name.empty())
      dev.name = dev.mac;
    dev.paired = true;
    target.push_back(dev);
  }
}

// ---------------------------------------------------------------- worker ----
void BluetoothManager::workerLoop() {
  auto last_scan_poll = std::chrono::steady_clock::now();
  auto last_auto = std::chrono::steady_clock::now();
  while (true) {
    Request r;
    bool have = false;
    {
      std::unique_lock<std::mutex> l(mtx_);
      cv_.wait_for(l, std::chrono::milliseconds(250),
                   [this] { return stop_ || !queue_.empty(); });
      if (stop_)
        return;
      if (!queue_.empty()) {
        r = queue_.front();
        queue_.pop_front();
        have = true;
      }
    }
    if (have) {
      busy_ = true;
      execRequest(r);
      busy_ = false;
    }
    scanTick(last_scan_poll);
    if (auto_refresh_) {
      auto now = std::chrono::steady_clock::now();
      if (std::chrono::duration<double>(now - last_auto).count() >= 5.0) {
        last_auto = now;
        refreshCache();
      }
    }
  }
}

void BluetoothManager::refreshCache() {
  std::string out = runCmd("bluetoothctl devices Paired 2>/dev/null");
  if (out.empty())
    out = runCmd("bluetoothctl paired-devices 2>/dev/null");
  std::vector<BluetoothDevice> devs;
  parseDevicesOutput(out, devs);

  for (auto &d : devs) {
    // El estado de conexión solo lo da `info`; no se puede evitar.
    std::string i = runCmd("bluetoothctl info " + d.mac + " 2>/dev/null");
    d.connected = contains(i, "Connected: yes");

    // Clasificación: primero por nombre (gratis), luego caché, luego info.
    BtDeviceKind by_name = classifyByName(d.name);
    if (by_name != BtDeviceKind::Unknown) {
      d.kind = by_name;
      kind_cache_[d.mac] = by_name;
    } else {
      auto it = kind_cache_.find(d.mac);
      if (it != kind_cache_.end()) {
        d.kind = it->second;
      } else {
        d.kind = classifyFromInfo(i, d.name);
        kind_cache_[d.mac] = d.kind;
      }
    }
  }

  {
    std::lock_guard<std::mutex> l(state_mtx_);
    devices_ = std::move(devs);
  }
  pushEvent({BtEventType::DevicesChanged, "", "", ""});
}

void BluetoothManager::execRequest(const Request &r) {
  if (r.op == "refresh") {
    refreshCache();
    return;
  }

  if (r.op == "scan") {
    {
      std::lock_guard<std::mutex> l(state_mtx_);
      if (scanning_)
        return;
      scanning_ = true;
      scan_timeout_sec_ = r.arg > 0 ? r.arg : 12;
      scan_start_ = std::chrono::steady_clock::now();
      discovered_.clear();
    }
    pid_t pid = fork();
    if (pid == 0) {
      freopen("/dev/null", "w", stdout);
      freopen("/dev/null", "w", stderr);
      std::string t = std::to_string(r.arg);
      execlp("bluetoothctl", "bluetoothctl", "--timeout", t.c_str(), "scan",
             "on", nullptr);
      _exit(127);
    }
    pushEvent({BtEventType::ScanStarted, "", "", ""});
    return;
  }

  if (r.op == "connect") {
    runCmd("bluetoothctl trust " + r.mac + " 2>&1");
    std::string out = runCmd("bluetoothctl connect " + r.mac + " 2>&1");
    SDL_Log("[ludex] BT connect %s: %s", r.mac.c_str(), out.c_str());
    BtEvent e;
    e.mac = r.mac;
    e.name = nameFor(r.mac);
    if (contains(out, "successful") || contains(out, "Already connected"))
      e.type = BtEventType::ConnectOk;
    else if (contains(out, "AuthenticationFailed") ||
             contains(out, "Authentication Rejected") ||
             contains(out, "not authorized"))
      e.type = BtEventType::ConnectNeedsPin;
    else {
      e.type = BtEventType::ConnectFailed;
      e.detail = firstFailedLine(out);
    }
    pushEvent(e);
    refreshCache();
    return;
  }

  if (r.op == "disconnect") {
    std::string out =
        lowerCopy(runCmd("bluetoothctl disconnect " + r.mac + " 2>&1"));
    BtEvent e;
    e.mac = r.mac;
    e.name = nameFor(r.mac);
    e.type = out.find("successful") != std::string::npos
                 ? BtEventType::DisconnectOk
                 : BtEventType::DisconnectFailed;
    pushEvent(e);
    refreshCache();
    return;
  }
  if (r.op == "remove") {
    BtEvent e;
    e.mac = r.mac;
    e.name = nameFor(r.mac);
    std::string out =
        lowerCopy(runCmd("bluetoothctl remove " + r.mac + " 2>&1"));
    bool ok = out.find("has been removed") != std::string::npos ||
              out.find("successful") != std::string::npos;
    e.type = ok ? BtEventType::RemoveOk : BtEventType::RemoveFailed;
    pushEvent(e);
    refreshCache();
    return;
  }

  if (r.op == "pair") {
    openPinSession(r.mac);
    if (pin_pid_ < 0) {
      pushEvent(
          {BtEventType::PairFailed, r.mac, nameFor(r.mac), "fork failed"});
      return;
    }
    writeSessionLine("pair " + r.mac);
    runPinSession(r.mac, "");
    return;
  }

  if (r.op == "pair_pin") {
    openPinSession(r.mac);
    if (pin_pid_ < 0) {
      pushEvent(
          {BtEventType::PairFailed, r.mac, nameFor(r.mac), "fork failed"});
      return;
    }
    writeSessionLine("pair " + r.mac);
    runPinSession(r.mac, r.pin);
    return;
  }
  if (r.op == "scan_off") {
    runCmd("bluetoothctl scan off 2>&1");
    return;
  }
}

void BluetoothManager::scanTick(
    std::chrono::steady_clock::time_point &last_poll) {
  bool scanning;
  std::chrono::steady_clock::time_point start;
  int timeout;
  {
    std::lock_guard<std::mutex> l(state_mtx_);
    scanning = scanning_;
    start = scan_start_;
    timeout = scan_timeout_sec_;
  }
  if (!scanning)
    return;
  auto now = std::chrono::steady_clock::now();
  double elapsed = std::chrono::duration<double>(now - start).count();
  if (elapsed >= timeout) {
    {
      std::lock_guard<std::mutex> l(state_mtx_);
      scanning_ = false;
    }
    pushEvent({BtEventType::ScanFinished, "", "", ""});
    refreshCache();
    return;
  }
  if (std::chrono::duration<double>(now - last_poll).count() >= 3.0) {
    last_poll = now;
    std::string out = runCmd("bluetoothctl devices 2>/dev/null");
    std::vector<BluetoothDevice> all;
    parseDevicesOutput(out, all);
    {
      std::lock_guard<std::mutex> l(state_mtx_);
      discovered_.clear();
      for (auto &d : all) {
        bool paired = std::any_of(
            devices_.begin(), devices_.end(),
            [&](const BluetoothDevice &p) { return p.mac == d.mac; });
        if (!paired) {
          d.paired = false;
          d.kind = classifyByName(d.name);
          discovered_.push_back(d);
        }
      }
    }
    pushEvent({BtEventType::DevicesChanged, "", "", ""});
  }
}

// ------------------------- coproceso interactivo (PIN) ----------------------
void BluetoothManager::openPinSession(const std::string &mac) {
  closePinSession();
  pin_cancel_ = false;
  int in_pipe[2], out_pipe[2];
  if (pipe(in_pipe) != 0 || pipe(out_pipe) != 0)
    return;
  pid_t pid = fork();
  if (pid == 0) {
    dup2(in_pipe[0], 0);
    dup2(out_pipe[1], 1);
    dup2(out_pipe[1], 2);
    close(in_pipe[0]);
    close(in_pipe[1]);
    close(out_pipe[0]);
    close(out_pipe[1]);
    execlp("bluetoothctl", "bluetoothctl", nullptr); // interactivo
    _exit(127);
  }
  close(in_pipe[0]);
  close(out_pipe[1]);
  pin_in_fd_ = in_pipe[1];
  pin_out_fd_ = out_pipe[0];
  pin_pid_ = pid;
  pin_mac_ = mac;
  sess_buf_.clear();
  writeSessionLine("agent KeyboardDisplay");
  writeSessionLine("default-agent");
}

void BluetoothManager::closePinSession() {
  if (pin_in_fd_ >= 0) {
    ::close(pin_in_fd_);
    pin_in_fd_ = -1;
  }
  if (pin_out_fd_ >= 0) {
    ::close(pin_out_fd_);
    pin_out_fd_ = -1;
  }
  if (pin_pid_ > 0) {
    ::kill(pin_pid_, SIGTERM);
    ::waitpid(pin_pid_, nullptr, 0);
    pin_pid_ = -1;
  }
  sess_buf_.clear();
  std::lock_guard<std::mutex> l(state_mtx_);
  awaiting_pin_ = false;
}

void BluetoothManager::writeSessionLine(const std::string &s) {
  std::lock_guard<std::mutex> l(state_mtx_);
  if (pin_in_fd_ < 0)
    return;
  std::string out = s + "\n";
  (void)::write(pin_in_fd_, out.data(), out.size());
}

bool BluetoothManager::readSessionLine(std::string &line, int timeout_ms) {
  size_t pos;
  if ((pos = sess_buf_.find('\n')) != std::string::npos) {
    line = sess_buf_.substr(0, pos);
    sess_buf_.erase(0, pos + 1);
    if (!line.empty() && line.back() == '\r')
      line.pop_back();
    return true;
  }
  if (pin_out_fd_ < 0)
    return false;
  struct pollfd pfd;
  pfd.fd = pin_out_fd_;
  pfd.events = POLLIN;
  pfd.revents = 0;
  int pr = ::poll(&pfd, 1, timeout_ms);
  if (pr <= 0)
    return false;
  char buf[512];
  ssize_t n = ::read(pin_out_fd_, buf, sizeof(buf));
  if (n <= 0) {
    pin_out_fd_ = -2;
    return false;
  } // EOF
  sess_buf_.append(buf, (size_t)n);
  if ((pos = sess_buf_.find('\n')) != std::string::npos) {
    line = sess_buf_.substr(0, pos);
    sess_buf_.erase(0, pos + 1);
    if (!line.empty() && line.back() == '\r')
      line.pop_back();
    return true;
  }
  return false;
}

void BluetoothManager::runPinSession(const std::string &mac,
                                     std::string pending_pin) {
  auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(60);
  bool done = false;
  while (!done && std::chrono::steady_clock::now() < deadline) {
    if (pin_cancel_) {
      pushEvent({BtEventType::PairFailed, mac, nameFor(mac), "cancelled"});
      break;
    }
    std::string line;
    if (!readSessionLine(line, 200)) {
      if (pin_out_fd_ == -2) { // bluetoothctl murió
        pushEvent({BtEventType::PairFailed, mac, nameFor(mac),
                   "bluetoothctl exited"});
        break;
      }
      continue;
    }
    SDL_Log("[ludex] BT session: %s", line.c_str());
    if (contains(line, "Request PIN code") ||
        contains(line, "Enter PIN code") || contains(line, "Request passkey") ||
        contains(line, "Enter passkey")) {
      if (!pending_pin.empty()) {
        writeSessionLine(pending_pin);
        pending_pin.clear();
      } else {
        {
          std::lock_guard<std::mutex> l(state_mtx_);
          awaiting_pin_ = true;
        }
        pushEvent({BtEventType::PairNeedsPin, mac, nameFor(mac), ""});
      }
    } else if (contains(line, "Confirm passkey")) {
      writeSessionLine("yes");
    } else if (contains(line, "Pairing successful") ||
               contains(line, "Already paired")) {
      pushEvent({BtEventType::PairOk, mac, nameFor(mac), ""});
      runCmd("bluetoothctl trust " + mac + " 2>&1");
      std::string c = runCmd("bluetoothctl connect " + mac + " 2>&1");
      BtEvent e;
      e.mac = mac;
      e.name = nameFor(mac);
      e.type = (contains(c, "successful") || contains(c, "Already connected"))
                   ? BtEventType::ConnectOk
                   : BtEventType::ConnectFailed;
      if (e.type == BtEventType::ConnectFailed)
        e.detail = firstFailedLine(c);
      pushEvent(e);
      done = true;
    } else if (contains(line, "AuthenticationFailed") ||
               contains(line, "rejected")) {
      pushEvent({BtEventType::PairFailed, mac, nameFor(mac), line});
      done = true;
    }
  }
  closePinSession();
  refreshCache();
}

void BluetoothManager::cancelScan() {
  {
    std::lock_guard<std::mutex> l(state_mtx_);
    if (!scanning_)
      return;
    scanning_ = false;
  }
  enqueue({"scan_off", "", ""});
}

void BluetoothManager::requestCancelScan() {
  bool was_scanning = false;
  {
    std::lock_guard<std::mutex> l(state_mtx_);
    if (scanning_) {
      scanning_ = false;
      was_scanning = true;
    }
  }

  if (was_scanning) {
    // Avisamos a la UI para que oculte el texto de "SCANNING..."
    pushEvent({BtEventType::ScanFinished, "", "", "cancelled"});
    refreshCache();
  }
}