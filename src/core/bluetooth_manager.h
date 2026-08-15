#pragma once
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

enum class BtDeviceKind { Unknown, Gamepad, Audio };

struct BluetoothDevice {
  std::string mac, name;
  bool paired = false, connected = false;
  BtDeviceKind kind = BtDeviceKind::Unknown;
};

enum class BtEventType {
  ScanStarted, ScanFinished,
  ConnectOk, ConnectFailed, ConnectNeedsPin,
  DisconnectOk, DisconnectFailed,
  PairOk, PairFailed, PairNeedsPin,
  RemoveOk, RemoveFailed,
  DevicesChanged
};

struct BtEvent {
  BtEventType type;
  std::string mac, name, detail;
};

class BluetoothManager {
public:
  BluetoothManager();
  ~BluetoothManager();
  void shutdown();

  bool available() const;

  // --- Peticiones asíncronas: NUNCA bloquean el hilo principal ---
  void requestRefresh();
  void requestScan(int timeout_sec = 15);
  void requestCancelScan();
  void requestConnect(const std::string &mac);
  void requestDisconnect(const std::string &mac);
  void requestPair(const std::string &mac);
  void requestRemove(const std::string &mac);
  void submitPin(const std::string &mac, const std::string &pin);
  void cancelPin();

  // --- Hilo principal: drenar resultados ---
  bool pollEvent(BtEvent &out);

  // --- Estado cacheado (copias thread-safe) ---
  std::vector<BluetoothDevice> devices() const;
  std::vector<BluetoothDevice> discoveredDevices() const;
  bool isScanning() const;
  int scanRemainingSec() const;
  bool isBusy() const { return busy_; }
  bool awaitingPin() const { return awaiting_pin_; }
  void setAutoRefresh(bool on);

private:
  struct Request { std::string op, mac, pin; int arg = 0; };

  void workerLoop();
  void execRequest(const Request &r);
  void scanTick(std::chrono::steady_clock::time_point &last_poll);
  void refreshCache();
  std::string nameFor(const std::string &mac);
  void pushEvent(BtEvent e);
  void enqueue(Request r);

  // coproceso interactivo de bluetoothctl para PIN/passkey
  void openPinSession(const std::string &mac);
  void closePinSession();
  void writeSessionLine(const std::string &s);
  bool readSessionLine(std::string &line, int timeout_ms);
  void runPinSession(const std::string &mac, std::string pending_pin);

  static std::string runCmd(const std::string &cmd);
  static void parseDevicesOutput(const std::string &out,
                                 std::vector<BluetoothDevice> &tgt);

  std::thread worker_;
  std::mutex mtx_;
  std::condition_variable cv_;
  std::deque<Request> queue_;
  std::deque<BtEvent> events_;
  bool stop_ = false;
  std::atomic<bool> busy_{false};
  std::atomic<bool> auto_refresh_{false};

  mutable std::mutex state_mtx_;
  std::vector<BluetoothDevice> devices_;
  std::vector<BluetoothDevice> discovered_;
  bool scanning_ = false;
  std::chrono::steady_clock::time_point scan_start_;
  int scan_timeout_sec_ = 0;
  bool awaiting_pin_ = false;

  int pin_in_fd_ = -1, pin_out_fd_ = -1;
  pid_t pin_pid_ = -1;
  std::string pin_mac_, sess_buf_;
  std::atomic<bool> pin_cancel_{false};
  void cancelScan();
};